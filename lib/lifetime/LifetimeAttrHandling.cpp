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
#include "cppsafe/util/type.h"

#include <clang/AST/Attr.h>
#include <clang/AST/Decl.h>
#include <clang/AST/DeclCXX.h>
#include <clang/AST/Expr.h>
#include <clang/AST/ExprCXX.h>
#include <clang/Basic/SourceLocation.h>
#include <gsl/narrow>

#include <map>
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

    // p2.5.2: Create the input sets
    // p2.5.4: Create the output sets
    void fillPreConditions(LifetimeContractAttr* ContractAttr, ParamDerivedLocations& Locations) const
    {
        // Fill default preconditions and collect data for
        // computing default postconditions.
        for (const ParmVarDecl* PVD : FD->parameters()) {
            const QualType ParamType = PVD->getType();
            const TypeClassification TC = classifyTypeCategory(ParamType);
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
            switch (classifyTypeCategory(PointeeType)) {
            case TypeCategory::Owner: {
                ContractAttr->PrePSets.emplace(ParamDerefLoc, ParamDerefPSet);
                if (ParamType->isLValueReferenceType()) {
                    if (PointeeType.isConstQualified()) {
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
                    && !isLifetimeIn(PVD)) {
                    // Output params are initially invalid.
                    ContractPSet InvalidPS;
                    InvalidPS.ContainsInvalid = true;
                    ContractAttr->PrePSets.emplace(ParamDerefLoc, InvalidPS);
                    addParamSet(Locations.Output, ParamDerefLoc);
                } else {
                    ContractAttr->PrePSets.emplace(ParamDerefLoc, ParamDerefPSet);
                    // TODO: In the paper we only add derefs for references and not for
                    // other pointers. Is this intentional?
                    if (ParamType->isLValueReferenceType()) {
                        addParamSet(Locations.Input, ParamDerefLoc);
                    }
                }
                LLVM_FALLTHROUGH;
            default:
                if (!ParamType->isRValueReferenceType()) {
                    addParamSet(Locations.Input, ParamLoc);
                }
                break;
            }
        }

        // This points to deref this and this considered as input.
        // If *this is an indirection, then *this is considered as input too
        if (const auto* MD = dyn_cast<CXXMethodDecl>(FD); MD && MD->isInstance()) {
            const auto* RD = dyn_cast<CXXRecordDecl>(MD->getParent());
            ContractVariable DerefThis = ContractVariable(RD).deref();
            ContractPSet ThisPSet({ DerefThis });
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

    void fillPostConditions(LifetimeContractAttr* ContractAttr, ParamDerivedLocations& Locations) const
    {
        // p2.5.5
        // Compute default postconditions.
        auto ComputeOutput = [&](QualType OutputType) {
            ContractPSet Ret;
            for (const ContractVariable& CV : Locations.Input) {
                if (canAssign(getLocationType(CV), OutputType)) {
                    Ret.merge(ContractAttr->PrePSets.at(CV));
                }
            }
            if (Ret.isEmpty()) {
                for (const ContractVariable& CV : Locations.InputWeak) {
                    if (canAssign(getLocationType(CV), OutputType)) {
                        Ret.merge(ContractAttr->PrePSets.at(CV));
                    }
                }
            }
            if (Ret.isEmpty()) {
                Ret.ContainsGlobal = true;
            }
            // For not_null types are never null regardless of type matching.
            Ret.ContainsNull = isNullableType(OutputType);
            return Ret;
        };

        if (classifyTypeCategory(FD->getReturnType()) == TypeCategory::Pointer) {
            Locations.Output.push_back(ContractVariable::returnVal());
        }

        for (const ContractVariable& CV : Locations.Output) {
            ContractAttr->PostPSets[CV] = ComputeOutput(getLocationType(CV));
        }

        // Process user defined postconditions.
        const auto Range = adjustContracts(LifetimePost, FD, ContractAttr->PostPSets, ContractAttr->PrePSets);
        if (Range.isValid()) {
            Reporter.warnUnsupportedExpr(Range);
        }
    }

    bool canAssign(QualType From, QualType To) const
    {
        QualType FromPointee = getPointeeType(From);
        if (FromPointee.isNull()) {
            return false;
        }

        QualType ToPointee = getPointeeType(To);
        if (ToPointee.isNull()) {
            return false;
        }

        return IsConvertible(ASTCtxt.getPointerType(FromPointee), ASTCtxt.getPointerType(ToPointee));
    }

    QualType getLocationType(const ContractVariable& CV) const
    {
        if (CV == ContractVariable::returnVal()) {
            return FD->getReturnType();
        }
        return Variable(CV, FD).getType();
    }

    static void addParamSet(std::vector<ContractVariable>& To, const ContractVariable& Var) { To.push_back(Var); }

    const FunctionDecl* FD;
    const ASTContext& ASTCtxt;
    IsConvertibleTy IsConvertible;
    LifetimeReporterBase& Reporter;
};

} // anonymous namespace

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

void getLifetimeContracts(PSetsMap& PMap, const FunctionDecl* FD, const ASTContext& ASTCtxt, const CFGBlock* Block,
    IsConvertibleTy IsConvertible, LifetimeReporterBase& Reporter, bool Pre)
{
    auto* ContractAttr = getLifetimeContracts(FD);

    // TODO: this check is insufficient for functions like int f(int);
    if (ContractAttr->PrePSets.empty() && ContractAttr->PostPSets.empty()) {
        PSetCollector Collector(FD, ASTCtxt, IsConvertible, Reporter);
        Collector.fillPSetsForDecl(ContractAttr);
    }

    if (Pre) {
        for (const auto& Pair : ContractAttr->PrePSets) {
            Variable V(Pair.first, FD);
            PSet PS(Pair.second, FD);
            if (const auto* PVD = dyn_cast_or_null<ParmVarDecl>(V.asVarDecl())) {
                if (!V.isField() && !V.isDeref() && PS.containsNull()) {
                    PS.addNullReason(NullReason::parameterNull(PVD->getSourceRange(), Block));
                }
                if (PS.containsInvalid()) {
                    PS = PSet::invalid(InvalidationReason::NotInitialized(PVD->getSourceRange(), Block));
                }
            }
            PMap.emplace(V, PS);
        }
    } else {
        for (const auto& Pair : ContractAttr->PostPSets) {
            PMap.emplace(Variable(Pair.first, FD), PSet(Pair.second, FD));
        }
    }
}

} // namespace clang::lifetime
