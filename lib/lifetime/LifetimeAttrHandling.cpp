//=- LifetimeAttrHandling.cpp - Diagnose lifetime violations -*- C++ -*-======//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//

#include "cppsafe/lifetime/Attr.h"
#include "cppsafe/lifetime/Lifetime.h"
#include "cppsafe/lifetime/LifetimeAttrData.h"
#include "cppsafe/lifetime/LifetimePset.h"
#include "cppsafe/lifetime/LifetimePsetBuilder.h"
#include "cppsafe/lifetime/LifetimeTypeCategory.h"
#include "cppsafe/lifetime/contract/Annotation.h"
#include "cppsafe/lifetime/contract/Parser.h"
#include "cppsafe/lifetime/type/Aggregate.h"
#include "cppsafe/util/type.h"

#include <clang/AST/Attr.h>
#include <clang/AST/Decl.h>
#include <clang/AST/DeclCXX.h>
#include <clang/AST/Expr.h>
#include <clang/AST/ExprCXX.h>
#include <clang/AST/Type.h>
#include <clang/Basic/LLVM.h>
#include <clang/Basic/OperatorKinds.h>
#include <clang/Basic/SourceLocation.h>
#include <gsl/util>
#include <range/v3/range/conversion.hpp>
#include <range/v3/view/transform.hpp>

#include <functional>
#include <map>
#include <set>
#include <vector>

namespace clang::lifetime {

namespace {
class PSetCollector {
public:
    PSetCollector(const FunctionDecl* FD, const ASTContext& ASTCtxt, IsConvertibleTy IsConvertible,
        LifetimeReporterBase& Reporter)
        : FD(FD->getCanonicalDecl())
        , ASTCtxt(ASTCtxt)
        , IsConvertible(IsConvertible)
        , Reporter(Reporter)
    {
    }

    ~PSetCollector() = default;

    DISALLOW_COPY_AND_MOVE(PSetCollector);

    /// Fill the default annotations for a function and interpret the
    /// user-provided expressions to create the ContractVariable based
    /// representation of lifetime contracts. Both the expressions and the
    /// ContractVariables are stored in the annotation. The former is only read to
    /// build the latter.
    void fillPSetsForDecl(LifetimeContractAttr* ContractAttr) const
    {
        if (FD->isMain()) {
            if (FD->getNumParams() >= 2) {
                const ContractVariable V(FD->getParamDecl(1));
                const ContractVariable DerefV = V.derefCopy();
                ContractAttr->PrePSets.emplace(V, DerefV);
                ContractAttr->PrePSets.emplace(DerefV, DerefV.derefCopy());
            }
            return;
        }

        ParamDerivedLocations Locations;

        fillPreConditions(ContractAttr, Locations);
        fillPostConditions(ContractAttr, Locations);
    }

private:
    struct ParamDerivedLocations {
        std::vector<ContractVariable> InputWeak;
        std::vector<ContractVariable> Input;
        std::vector<ContractVariable> Output;
    };

    // p2.5.1: Expand parameters and returns: Aggregates, nonstatic data members, and Pointer dereference locations
    // p2.5.2: Create the input sets
    // p2.5.4: Create the output sets
    // NOLINTNEXTLINE(readability-function-cognitive-complexity)
    void fillPreConditions(LifetimeContractAttr* ContractAttr, ParamDerivedLocations& Locations) const
    {
        // Fill default preconditions and collect data for
        // computing default postconditions.
        for (const ParmVarDecl* PVD : FD->parameters()) {
            const QualType ParamType = PVD->getType();
            const TypeClassification TC = classifyTypeCategory(ParamType);

// value expansion is not handled well
#ifdef NOT_IMPLEMENTED
            if (TC.isAggregate()) {
                expandParameter(
                    ContractVariable(PVD), ParamType, ParamType, nullptr, ContractAttr->PrePSets, Locations);
            }
#endif
            if (!TC.isIndirection()) {
                continue;
            }

            const ContractVariable ParamLoc(PVD);
            const ContractVariable ParamDerefLoc(PVD, 1);
            // Nullable owners are a future note in the paper.
            ContractAttr->PrePSets.emplace(ParamLoc, ContractPSet { { ParamDerefLoc }, isNullableType(ParamType) });
            if (TC != TypeCategory::Pointer) {
                continue;
            }

            const QualType PointeeType = getPointeeType(ParamType);
            const ContractPSet ParamDerefPSet { { ContractVariable { PVD, 2 } }, isNullableType(PointeeType) };
            switch (classifyTypeCategory(PointeeType).TC) {
            case TypeCategory::Owner: {
                ContractAttr->PrePSets.emplace(ParamDerefLoc, ParamDerefPSet);
                if (!ParamType->isRValueReferenceType()) {
                    if (ParamType->isLValueReferenceType() && PointeeType.isConstQualified()) {
                        addParamSet(Locations.InputWeak, ParamLoc);
                        addParamSet(Locations.InputWeak, ParamDerefLoc);
                    } else {
                        addParamSet(Locations.Input, ParamLoc);
                        addParamSet(Locations.Input, ParamDerefLoc);
                    }
                }
                break;
            }
            case TypeCategory::Pointer:
                if (!isLifetimeConst(FD, PointeeType, gsl::narrow_cast<int>(PVD->getFunctionScopeIndex()))
                    && !isLifetimeIn(PVD)
                    // All parameters passed by lvalue reference to non-const
                    && !isForwardingReference(FD, PVD)) {
                    // Output params are initially invalid.
                    ContractPSet OutParamPS;
                    if (PointeeType->getAsCXXRecordDecl() || isLifetimeInout(PVD)) {
                        OutParamPS = ParamDerefPSet;
                        OutParamPS.ContainsNull = isNullableType(PointeeType);
                        addParamSet(Locations.Input, ParamDerefLoc);
                    } else {
                        OutParamPS.ContainsInvalid = true;
                    }
                    ContractAttr->PrePSets.emplace(ParamDerefLoc, OutParamPS);
                    addParamSet(Locations.Output, ParamDerefLoc);
                } else {
                    ContractAttr->PrePSets.emplace(ParamDerefLoc, ParamDerefPSet);
                    // TODO: In the paper we only add derefs for references and not for
                    // other pointers. Is this intentional?
                    if (!ParamType->isRValueReferenceType()) {
                        addParamSet(Locations.Input, ParamDerefLoc);
                    }
                }
                if (!ParamType->isRValueReferenceType()) {
                    addParamSet(Locations.Input, ParamLoc);
                }
                break;

            case TypeCategory::Aggregate:
            case TypeCategory::Value:
                if (PointeeType->isRecordType()) {
                    expandParameter(ParamDerefLoc, PointeeType, PointeeType, ContractAttr->PrePSets.at(ParamLoc),
                        ContractAttr->PrePSets, Locations);
                }

                if (!ParamType->isRValueReferenceType()) {
                    addParamSet(Locations.Input, ParamLoc);
                }
                break;
            }
        }

        // This points to deref this and this considered as input.
        // If *this is an indirection, then *this is considered as input too
        if (const auto* MD = dyn_cast<CXXMethodDecl>(FD); MD && MD->isInstance() && !isa<CXXDestructorDecl>(MD)) {
            const auto* RD = dyn_cast<CXXRecordDecl>(MD->getParent());
            const ContractVariable DerefThis = ContractVariable(RD).deref();
            const ContractPSet ThisPSet({ DerefThis });
            ContractAttr->PrePSets.emplace(ContractVariable(RD), ThisPSet);
            addParamSet(Locations.Input, ContractVariable(RD));

            const QualType ClassTy = MD->getThisType()->getPointeeType();
            const TypeClassification TC = classifyTypeCategory(ClassTy);
            if (TC.isIndirection()) {
                ContractPSet DerefThisPSet({ ContractVariable(RD).deref(2) });

                auto OO = MD->getOverloadedOperator();
                if (OO != OverloadedOperatorKind::OO_Star && OO != OverloadedOperatorKind::OO_Arrow
                    && OO != OverloadedOperatorKind::OO_ArrowStar && OO != OverloadedOperatorKind::OO_Subscript) {
                    DerefThisPSet.ContainsNull = isNullableType(ClassTy);
                }
                if (const auto* Conv = dyn_cast<CXXConversionDecl>(MD)) {
                    DerefThisPSet.ContainsNull |= Conv->getConversionType()->isBooleanType();
                }

                ContractAttr->PrePSets.emplace(DerefThis, DerefThisPSet);
                addParamSet(Locations.Input, DerefThis);
            }

            // Do not expand owners, they are the basic building block
            if (!TC.isOwner()) {
                // If the type of *this is not an Aggregate, treat each nonstatic data member m of *this as a parameter
                // passed by value if in a constructor (and on function entry they are treated as described in §2.4.2)
                // or by reference otherwise (and on function entry they are treated as described in this section).
                expandParameter(DerefThis, ClassTy, ClassTy, ThisPSet, ContractAttr->PrePSets, Locations);
            }
        }

        // Adust preconditions based on annotations.
        const auto Range = adjustContracts(LifetimePre, FD, ContractAttr->PrePSets, ContractAttr->PrePSets);
        if (Range.isValid()) {
            Reporter.warnUnsupportedExpr(Range);
        }

        for (const auto* PVD : FD->parameters()) {
            const auto Range
                = adjustParamContracts(LifetimePre, FD, PVD, ContractAttr->PrePSets, ContractAttr->PrePSets);
            if (Range.isValid()) {
                Reporter.warnUnsupportedExpr(Range);
            }
        }
    }

    // p2.5.1: Expand parameters and returns: Aggregates, nonstatic data members, and Pointer dereference locations
    //
    // Now we only expand parameters passed by pointer
    //
    // NOLINTNEXTLINE(readability-function-cognitive-complexity)
    void expandParameter(const ContractVariable& V, const QualType CurrentTy, const QualType RootTy,
        const ContractPSet& RootPset, LifetimeContractAttr::PointsToMap& PMap, ParamDerivedLocations& Locations) const
    {
        const auto RootTC = classifyTypeCategory(RootTy);

        for (const auto* Member : CurrentTy->getAsCXXRecordDecl()->fields()) {
            ContractVariable MemberVar = V;
            MemberVar.addFieldRef(Member);

            const auto MemberTC = classifyTypeCategory(Member->getType());
            if (MemberTC.isAggregate() || (MemberTC.isPointer() && Member->getType()->getAsCXXRecordDecl())) {
                PMap.emplace(MemberVar, RootPset);
                if (!RootTy->isRValueReferenceType()) {
                    addParamSet(Locations.Input, MemberVar);
                }

                // Apply recursively to nested Aggregates’ by-value data members
                // Ex: if member is a Pointer record, expand it
                expandParameter(MemberVar, Member->getType(), RootTy, RootPset, PMap, Locations);
                continue;
            }

            if (MemberTC.isPointer()) {
                if (RootTC.isPointer()) {
                    // struct Pointer {
                    //     char* data_;
                    //     char* data() const { return data_; }
                    // };
                    // pset((*this).data_) = {**this}
                    // it's not in INPUT
                    PMap.emplace(MemberVar, derefPset(RootPset));
                    continue;
                }

                // If Root is Aggregate, and when expand a pointer member, assume its pset is deref itself
                // Ignore the default behavior
                continue;
            }

            // FD is owner or value
            // 2.5.3
            if (MemberTC.isValue()) {
                PMap.emplace(MemberVar, RootPset);
                if (!RootTy->isRValueReferenceType()) {
                    addParamSet(Locations.Input, MemberVar);
                }
                continue;
            }

            // for owner members, add pset(*agg) = {**agg}
            for (const auto& V : RootPset.Vars) {
                PMap.emplace(V, V.derefCopy());
            }

            // foo(Owner& agg_m): pset(agg_m) = {*agg}
            PMap.emplace(MemberVar, RootPset);

            if (!RootTy->isRValueReferenceType()) {
                if (RootTy->isLValueReferenceType() && RootTy.isConstQualified()) {
                    addParamSet(Locations.InputWeak, MemberVar);
                } else {
                    addParamSet(Locations.Input, MemberVar);
                }
            }
        }
    }

    static ContractPSet derefPset(const ContractPSet& P)
    {
        ContractPSet R = P;
        R.Vars = P.Vars | ranges::views::transform([](const ContractVariable& V) { return V.derefCopy(); })
            | ranges::to<std::set>();
        return R;
    }

    void fillPostConditions(LifetimeContractAttr* ContractAttr, ParamDerivedLocations& Locations) const
    {
        const auto RetTC = classifyTypeCategory(FD->getReturnType());
        if (RetTC.isPointer()) {
            Locations.Output.push_back(ContractVariable::returnVal(FD));
        } else if (RetTC.isAggregate()) {
            expandAggregate(Variable::returnVal(FD), FD->getReturnType()->getAsCXXRecordDecl(),
                [&Locations](const Variable& SubObj, const SubVarPath& Path, TypeClassification) {
                    if (Path.empty()) {
                        return;
                    }

                    if (!classifyTypeCategory(Path.back()->getType()).isPointer()) {
                        return;
                    }

                    Locations.Output.push_back(SubObj);
                });
        }

        for (const ContractVariable& CV : Locations.Output) {
            ContractAttr->PostPSets[CV] = computeOutput(ContractAttr, Locations, getLocationType(CV));
        }

        // Process user defined postconditions.
        auto Range = adjustContracts(LifetimePost, FD, ContractAttr->PostPSets, ContractAttr->PrePSets);
        if (Range.isValid()) {
            Reporter.warnUnsupportedExpr(Range);
        }

        Range = adjustCaptureContracts(FD, ContractAttr->PostPSets, ContractAttr->PrePSets);
        if (Range.isValid()) {
            Reporter.warnUnsupportedExpr(Range);
        }
    }

    // p2.5.5
    // Compute default postconditions.
    // NOLINTNEXTLINE(readability-function-cognitive-complexity)
    ContractPSet computeOutput(
        LifetimeContractAttr* ContractAttr, ParamDerivedLocations& Locations, QualType OutputType) const
    {
        ContractPSet Ret;
        for (const ContractVariable& CV : Locations.Input) {
            if (!CV.isMemberExpansion()) {
                if (canAssign(getLocationType(CV), OutputType)) {
                    Ret.merge(ContractAttr->PrePSets.at(CV));
                }
                continue;
            }
            if (!Ret.isEmpty()) {
                continue;
            }

            // consider int* foo(Aggr* aggr) expansion:
            //  int* foo(Owner* aggr.owner)
            //  int* foo(int* aggr.val)
            //  Owner* foo(Owner* aggr.owner)
            if (canAssign(getLocationType(CV), OutputType) || canAssign(getMemberLocationType(CV), OutputType)) {
                Ret.merge(ContractAttr->PrePSets.at(CV));
            }
        }
        if (Ret.isEmpty()) {
            for (const ContractVariable& CV : Locations.InputWeak) {
                if (!CV.isMemberExpansion()) {
                    if (canAssign(getLocationType(CV), OutputType)) {
                        Ret.merge(ContractAttr->PrePSets.at(CV));
                    }
                    continue;
                }
                if (!Ret.isEmpty()) {
                    continue;
                }

                if (canAssign(getLocationType(CV), OutputType) || canAssign(getMemberLocationType(CV), OutputType)) {
                    Ret.merge(ContractAttr->PrePSets.at(CV));
                }
            }
        }
        if (Ret.isEmpty()) {
            // nullness is handled later
            // think of pset(malloc()) = {null, global}
            Ret.ContainsGlobal = true;
        }

        // For not_null types are never null regardless of type matching.
        Ret.ContainsNull = isNullableType(OutputType);
        return Ret;
    }

    static bool isForwardingReference(const FunctionDecl* FD, const ParmVarDecl* PVD)
    {
        const auto* FunctionTemplate = FD->getPrimaryTemplate();
        if (!FunctionTemplate) {
            return PVD->getType()->isRValueReferenceType();
        }

        FD = FunctionTemplate->getTemplatedDecl();
        const unsigned Idx = PVD->getFunctionScopeIndex();
        const auto* D = Idx < FD->param_size() ? FD->getParamDecl(Idx) : FD->getParamDecl(FD->param_size() - 1);
        QualType ToCheck = D->getType();
        if (const auto* ParamExpansion = dyn_cast<PackExpansionType>(ToCheck)) {
            ToCheck = ParamExpansion->getPattern();
        }

        // C++1z [temp.deduct.call]p3:
        //   A forwarding reference is an rvalue reference to a cv-unqualified
        //   template parameter that does not represent a template parameter of a
        //   class template.
        if (const auto* ParamRef = ToCheck->getAs<RValueReferenceType>()) {
            if (ParamRef->getPointeeType().getQualifiers()) {
                return false;
            }

            const unsigned FirstInnerIndex = getFirstInnerIndex(FunctionTemplate);
            const auto* TypeParm = ParamRef->getPointeeType()->getAs<TemplateTypeParmType>();
            return TypeParm && TypeParm->getIndex() >= FirstInnerIndex;
        }
        return false;
    }

    static unsigned getFirstInnerIndex(const FunctionTemplateDecl* FTD)
    {
        auto* Guide = dyn_cast<CXXDeductionGuideDecl>(FTD->getTemplatedDecl());
        if (!Guide || !Guide->isImplicit()) {
            return 0;
        }
        return Guide->getDeducedTemplate()->getTemplateParameters()->size();
    }

    bool canAssign(const QualType From, const QualType To) const
    {
        const QualType FromPointee = getPointeeType(From);
        if (FromPointee.isNull()) {
            return false;
        }

        const QualType ToPointee = getPointeeType(To);
        if (ToPointee.isNull()) {
            return false;
        }

        return IsConvertible(ASTCtxt.getPointerType(FromPointee), ASTCtxt.getPointerType(ToPointee));
    }

    QualType getLocationType(const ContractVariable& CV) const
    {
        if (CV == ContractVariable::returnVal(FD)) {
            return FD->getReturnType();
        }

        return Variable(CV, FD).getType();
    }

    QualType getMemberLocationType(const ContractVariable& CV) const
    {
        const Variable V = Variable(CV, FD);
        QualType Ty = V.getType();

        // consider foo(Agg*) -> foo(int* agg_m)
        if (!classifyTypeCategory(Ty).isPointer()) {
            const QualType Base = V.getBaseType();

            if (const auto PT = Base->getPointeeType(); !PT.isNull()) {
                const bool IsConst = std::invoke([&V, PT, this] {
                    if (PT.isConstQualified()) {
                        return true;
                    }
                    const auto* MD = dyn_cast<CXXMethodDecl>(FD);
                    return V.isThisPointer() && MD && MD->isConst();
                });

                if (IsConst) {
                    Ty = ASTCtxt.getConstType(Ty);
                }
                Ty = ASTCtxt.getPointerType(Ty);
            }
        }

        return Ty;
    }

    static void addParamSet(std::vector<ContractVariable>& To, const ContractVariable& Var) { To.push_back(Var); }

    const FunctionDecl* FD;
    const ASTContext& ASTCtxt;
    IsConvertibleTy IsConvertible;
    LifetimeReporterBase& Reporter;
};

} // anonymous namespace

static ContractPSet adjustPSet(const ContractPSet& P, const CXXMethodDecl* Derived)
{
    auto RetP = P;

    RetP.Vars
        = P.Vars | ranges::views::transform([=](const auto& V) { return V.replace(Derived); }) | ranges::to<std::set>();

    return RetP;
}

static void copyContract4Derived(
    const LifetimeContractAttr* BaseAttr, const CXXMethodDecl* Derived, LifetimeContractAttr* DerivedAttr)
{
    for (const auto& [V, P] : BaseAttr->PrePSets) {
        DerivedAttr->PrePSets[V.replace(Derived)] = adjustPSet(P, Derived);
    }

    for (const auto& [V, P] : BaseAttr->PostPSets) {
        DerivedAttr->PostPSets[V.replace(Derived)] = adjustPSet(P, Derived);
    }
}

static LifetimeContractAttr* getLifetimeContracts(const FunctionDecl* FD)
{
    static std::map<const FunctionDecl*, LifetimeContractAttr> ContractCache;

    FD = FD->getCanonicalDecl();
    auto It = ContractCache.find(FD);
    if (It == ContractCache.end()) {
        It = ContractCache.emplace(FD, LifetimeContractAttr {}).first;
    }

    return &It->second;
}

static LifetimeContractAttr* getLifetimeContracts(
    const FunctionDecl* FD, const ASTContext& ASTCtxt, IsConvertibleTy IsConvertible, LifetimeReporterBase& Reporter)
{
    auto* ContractAttr = getLifetimeContracts(FD);

    if (!ContractAttr->Filled) {
        const auto* BaseMD = std::invoke([=]() -> const CXXMethodDecl* {
            const auto* MD = dyn_cast<CXXMethodDecl>(FD);
            if (!MD || MD->overridden_methods().empty()) {
                return nullptr;
            }

            while (!MD->overridden_methods().empty()) {
                MD = *MD->begin_overridden_methods();
            }

            return MD;
        });

        if (BaseMD) {
            copyContract4Derived(
                getLifetimeContracts(BaseMD, ASTCtxt, IsConvertible, Reporter), cast<CXXMethodDecl>(FD), ContractAttr);
        } else {
            const PSetCollector Collector(FD, ASTCtxt, IsConvertible, Reporter);
            Collector.fillPSetsForDecl(ContractAttr);
        }

        ContractAttr->Filled = true;
    }

    return ContractAttr;
}

// NOLINTNEXTLINE(readability-function-cognitive-complexity): refactor later
void getLifetimeContracts(PSetsMap& PMap, const FunctionDecl* FD, const ASTContext& ASTCtxt, const CFGBlock* Block,
    IsConvertibleTy IsConvertible, LifetimeReporterBase& Reporter, bool Pre, bool IgnoreNull, bool IgnoreFields)
{
    auto* ContractAttr = getLifetimeContracts(FD, ASTCtxt, IsConvertible, Reporter);

    if (Pre) {
        for (const auto& Pair : ContractAttr->PrePSets) {
            const Variable V(Pair.first, FD);
            if (IgnoreFields && V.isField() && V.isThisPointer()) {
                continue;
            }

            PSet PS(Pair.second, FD);
            if (const auto* PVD = dyn_cast_or_null<ParmVarDecl>(V.asVarDecl())) {
                // HACK: __builtin_va_arg
                const auto Range = PVD->getSourceRange().isValid() ? PVD->getSourceRange() : FD->getSourceRange();
                if (!V.isField() && !V.isDeref() && PS.containsNull()) {
                    PS.addNullReason(NullReason::parameterNull(Range, Block));
                }
                if (PS.containsInvalid()) {
                    PS = PSet::invalid(InvalidationReason::notInitialized(Range, Block));
                }
                if (IgnoreNull) {
                    // If -Wlifetime-null is enabled, all possible-null derefence will be flags
                    // We use -Wno-lifetime-call-null to exclude possible-null caused by parameters.
                    // When -Wno-lifetime-call-null is ON, suppose parameters are always non-null
                    //
                    // IgnoreNull will only be true when VisitFunction and -Wno-lifetime-call-null is ON
                    PS.removeNull();
                }
            }
            PMap.emplace(V, PS);
        }

        return;
    }

    for (const auto& Pair : ContractAttr->PostPSets) {
        PMap.emplace(Variable(Pair.first, FD), PSet(Pair.second, FD));
    }
}

} // namespace clang::lifetime
