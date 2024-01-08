//=- LifetimePsetBuilder.cpp - Diagnose lifetime violations -*- C++ -*-=======//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//

#include "cppsafe/lifetime/LifetimePsetBuilder.h"
#include "cppsafe/lifetime/Lifetime.h"
#include "cppsafe/lifetime/LifetimePset.h"
#include "cppsafe/lifetime/LifetimeTypeCategory.h"
#include "cppsafe/lifetime/contract/Annotation.h"
#include "cppsafe/lifetime/contract/CallVisitor.h"
#include "cppsafe/util/type.h"

#include <clang/AST/Attr.h>
#include <clang/AST/Decl.h>
#include <clang/AST/DeclCXX.h>
#include <clang/AST/DeclTemplate.h>
#include <clang/AST/Expr.h>
#include <clang/AST/ExprCXX.h>
#include <clang/AST/OperationKinds.h>
#include <clang/AST/Stmt.h>
#include <clang/AST/StmtVisitor.h>
#include <clang/AST/Type.h>
#include <clang/Analysis/CFG.h>
#include <clang/Basic/LLVM.h>
#include <clang/Basic/Lambda.h>
#include <clang/Basic/SourceLocation.h>
#include <clang/Lex/Lexer.h>
#include <llvm/ADT/APInt.h>
#include <llvm/ADT/STLExtras.h>
#include <llvm/ADT/StringSwitch.h>
#include <llvm/Support/ErrorHandling.h>
#include <llvm/Support/raw_ostream.h>

#include <cassert>
#include <functional>
#include <map>
#include <optional>
#include <string>
#include <utility>

namespace clang::lifetime {

namespace {

// NOLINTNEXTLINE(cppcoreguidelines-macro-usage): just for convevience
#define VERBOSE_DEBUG 0
#if VERBOSE_DEBUG
#define DBG(x) llvm::errs() << x
#else
#define DBG(x)
#endif

bool isPointer(const Expr* E)
{
    auto TC = classifyTypeCategory(E->getType());
    return TC == TypeCategory::Pointer;
}

/// Collection of methods to update/check PSets from statements/expressions
/// Conceptually, for each Expr where Expr::isLValue() is true,
/// we put an entry into the RefersTo map, which contains the set
/// of Variables that an lvalue might refer to, e.g.
/// RefersTo(var) = {var}
/// RefersTo(*p) = pset(p)
/// RefersTo(a = b) = {a}
/// RefersTo(a, b) = {b}
///
/// For every expression whose Type is a Pointer or an Owner,
/// we also track the pset (points-to set), e.g.
///  pset(&v) = {v}
///
/// In return statement, DeclRefExpr, LValueToRValueCast will move RefersTo to PSetsOfExpr
class PSetsBuilder final : public ConstStmtVisitor<PSetsBuilder, void>, public PSBuilder {
    const FunctionDecl* AnalyzedFD;
    LifetimeReporterBase& Reporter;
    ASTContext& ASTCtxt;
    /// Returns true if the first argument is implicitly convertible
    /// into the second argument.
    IsConvertibleTy IsConvertible;

    /// psets of all memory locations, which are identified
    /// by their non-reference variable declaration or
    /// MaterializedTemporaryExpr plus (optional) FieldDecls.
    PSetsMap& PMap;
    std::map<const Expr*, PSet>& PSetsOfExpr;
    std::map<const Expr*, PSet>& RefersTo;
    const CFGBlock* CurrentBlock = nullptr;

    /// Certain constructs that violate the type and bounds profile of the
    /// C++ core guidelines (such as reinterpret_cast) disable lifetime analysis.
    /// We disable it for the whole function because those constructs make it
    /// hard to asses which Pointers are tainted by them.
    bool AnalysisDisabled = false;

public:
    bool isAnalysisDisabled() const { return AnalysisDisabled; }

    /// Ignore parentheses and most implicit casts.
    /// Does not go through implicit cast that convert a literal into a pointer,
    /// because there the type category changes.
    /// Does not ignore LValueToRValue casts by default, because they
    /// move psets from RefersTo into PsetOfExpr.
    /// Does not ignore MaterializeTemporaryExpr as Expr::IgnoreParenImpCasts
    /// would.
    // NOLINTBEGIN(readability-else-after-return)
    static const Expr* ignoreTransparentExprs(const Expr* E, bool IgnoreLValueToRValue = false)
    {
        while (true) {
            E = E->IgnoreParens();
            if (const auto* P = dyn_cast<CastExpr>(E)) {
                switch (P->getCastKind()) {
                case CK_BitCast:
                case CK_LValueBitCast:
                case CK_IntegralToPointer:
                case CK_NullToPointer:
                case CK_ArrayToPointerDecay:
                    return E;
                case CK_LValueToRValue:
                    if (!IgnoreLValueToRValue) {
                        return E;
                    }
                    break;
                default:
                    break;
                }
                E = P->getSubExpr();
                continue;
            } else if (const auto* C = dyn_cast<ExprWithCleanups>(E)) {
                E = C->getSubExpr();
                continue;
            } else if (const auto* C = dyn_cast<OpaqueValueExpr>(E)) {
                E = C->getSourceExpr();
                continue;
            } else if (const auto* C = dyn_cast<UnaryOperator>(E)) {
                if (C->getOpcode() == UO_Extension) {
                    E = C->getSubExpr();
                    continue;
                }
            } else if (const auto* C = dyn_cast<CXXBindTemporaryExpr>(E)) {
                E = C->getSubExpr();
                continue;
            }
            return E;
        }
        // NOLINTEND(readability-else-after-return)
    }

    static bool isIgnoredStmt(const Stmt* S)
    {
        const Expr* E = dyn_cast<Expr>(S);
        return E && ignoreTransparentExprs(E) != E;
    }

    void VisitStringLiteral(const StringLiteral* SL) { setPSet(SL, PSet::globalVar(false)); }

    void VisitPredefinedExpr(const PredefinedExpr* E) { setPSet(E, PSet::globalVar(false)); }

    void VisitDeclStmt(const DeclStmt* DS)
    {
        for (const auto* DeclIt : DS->decls()) {
            if (const auto* VD = dyn_cast<VarDecl>(DeclIt)) {
                VisitVarDecl(VD);
            }
        }
    }

    void VisitCXXNewExpr(const CXXNewExpr* E) { setPSet(E, PSet::globalVar(false)); }

    void VisitAddrLabelExpr(const AddrLabelExpr* E) { setPSet(E, PSet::globalVar(false)); }

    void VisitCXXDefaultInitExpr(const CXXDefaultInitExpr* E)
    {
        if (hasPSet(E)) {
            setPSet(E, getPSet(E->getExpr()));
        }
    }

    PSet varRefersTo(const Variable& V, SourceRange Range) const
    {
        if (V.getType()->isReferenceType()) {
            auto P = getPSet(V);
            if (checkPSetValidity(P, Range)) {
                return P;
            }

            return {};
        }

        return PSet::singleton(V);
    }

    void VisitDeclRefExpr(const DeclRefExpr* DeclRef)
    {
        if (isa<FunctionDecl>(DeclRef->getDecl()) || DeclRef->refersToEnclosingVariableOrCapture()) {
            setPSet(DeclRef, PSet::globalVar(false));
        } else if (const auto* VD = dyn_cast<VarDecl>(DeclRef->getDecl())) {
            setPSet(DeclRef, varRefersTo(VD, DeclRef->getSourceRange()));
        } else if (const auto* B = dyn_cast<BindingDecl>(DeclRef->getDecl())) {
            // TODO: support BindingDecl in Variables?
            (void)B;
            setPSet(DeclRef, {});
        } else if (const auto* FD = dyn_cast<FieldDecl>(DeclRef->getDecl())) {
            Variable V = Variable::thisPointer(FD->getParent());
            V.deref(); // *this
            V.addFieldRef(FD); // this->field
            setPSet(DeclRef, varRefersTo(V, DeclRef->getSourceRange()));
        }
    }

    void VisitMemberExpr(const MemberExpr* ME)
    {
        const PSet BaseRefersTo = getPSet(ME->getBase());
        // Make sure that derefencing a dangling pointer is diagnosed unless
        // the member is a member function. In that case, the invalid
        // base will be diagnosed in VisitCallExpr().
        if (ME->getBase()->getType()->isPointerType() && !ME->hasPlaceholderType(BuiltinType::BoundMember)) {
            checkPSetValidity(BaseRefersTo, ME->getSourceRange());
        }

        if (auto* FD = dyn_cast<FieldDecl>(ME->getMemberDecl())) {
            PSet Ret = BaseRefersTo;
            Ret.addFieldRefIfTypeMatch(FD);
            if (FD->getType()->isReferenceType()) {
                // The field has reference type, apply the deref.
                Ret = derefPSet(Ret);
                if (!checkPSetValidity(Ret, ME->getSourceRange())) {
                    Ret = {};
                }
            }

            setPSet(ME, Ret);
        } else if (isa<VarDecl>(ME->getMemberDecl())) {
            // A static data member of this class
            setPSet(ME, PSet::globalVar(false));
        }
    }

    void VisitArraySubscriptExpr(const ArraySubscriptExpr* E)
    {
        // By the bounds profile, ArraySubscriptExpr is only allowed on arrays
        // (not on pointers).

        if (!E->getBase()->IgnoreParenImpCasts()->getType()->isArrayType()) {
            Reporter.warnPointerArithmetic(E->getSourceRange());
            AnalysisDisabled = true;
            setPSet(E, {});
            return;
        }

        setPSet(E, getPSet(E->getBase()));
    }

    void VisitCXXThisExpr(const CXXThisExpr* E)
    {
        // ThisExpr is an RValue. It points to *this.
        setPSet(E, PSet::singleton(Variable::thisPointer(E->getType()->getPointeeCXXRecordDecl()).deref()));
    }

    void VisitCXXTypeidExpr(const CXXTypeidExpr* E)
    {
        // The typeid expression is an lvalue expression which refers to an object
        // with static storage duration, of the polymorphic type const
        // std::type_info or of some type derived from it.
        setPSet(E, PSet::globalVar());
    }

    void VisitSubstNonTypeTemplateParmExpr(const SubstNonTypeTemplateParmExpr* E)
    {
        // Non-type template parameters that are pointers must point to something
        // static (because only addresses known at compiler time are allowed)
        if (hasPSet(E)) {
            setPSet(E, PSet::globalVar());
        }
    }

    void VisitAbstractConditionalOperator(const AbstractConditionalOperator* E)
    {
        if (!hasPSet(E)) {
            return;
        }
        // If the condition is trivially true/false, the corresponding branch
        // will be pruned from the CFG and we will not find a pset of it.
        // With AllowNonExisting, getPSet() will then return (unknown).
        // Note that a pset could also be explicitly unknown to suppress
        // further warnings after the first violation was diagnosed.
        auto LHS = getPSet(E->getTrueExpr(), /*AllowNonExisting=*/true);
        auto RHS = getPSet(E->getFalseExpr(), /*AllowNonExisting=*/true);
        setPSet(E, LHS + RHS);
    }

    void VisitMaterializeTemporaryExpr(const MaterializeTemporaryExpr* E)
    {
        const PSet Singleton = PSet::singleton(E);
        setPSet(E, Singleton);
        if (hasPSet(E->getSubExpr())) {
            auto TC = classifyTypeCategory(E->getSubExpr()->getType());
            if (TC == TypeCategory::Owner) {
                setPSet(Singleton, PSet::singleton(E, 1), E->getSourceRange());
            } else {
                setPSet(Singleton, getPSet(E->getSubExpr()), E->getSourceRange());
            }
        }
    }

    void VisitInitListExpr(const InitListExpr* I)
    {
        I = !I->isSemanticForm() ? I->getSemanticForm() : I;

        if (I->getType()->isPointerType()) {
            if (I->getNumInits() == 0) {
                setPSet(I, PSet::null(NullReason::defaultConstructed(I->getSourceRange(), CurrentBlock)));
                return;
            }
            // TODO: Instead of assuming that the pset comes from the first argument
            // use the same logic we have in call modelling.
            if (I->getNumInits() == 1) {
                setPSet(I, getPSet(I->getInit(0)));
                return;
            }
        }
        setPSet(I, {});
    }

    void VisitCXXScalarValueInitExpr(const CXXScalarValueInitExpr* E)
    {
        // Appears for `T()` in a template specialisation, where
        // T is a simple type.
        if (E->getType()->isPointerType()) {
            setPSet(E, PSet::null(NullReason::defaultConstructed(E->getSourceRange(), CurrentBlock)));
        }
    }

    void VisitCastExpr(const CastExpr* E)
    {
        // Some casts are transparent, see IgnoreTransparentExprs()
        switch (E->getCastKind()) {
        case CK_BitCast:
        case CK_LValueBitCast:
        case CK_IntegralToPointer:
            // Those casts are forbidden by the type profile
            Reporter.warnUnsafeCast(E->getSourceRange());
            AnalysisDisabled = true;
            setPSet(E, {});
            return;
        case CK_ArrayToPointerDecay:
            // Decaying an array into a pointer is like taking the address of the
            // first array member. The result is a pointer to the array elements,
            // which are '*array'.
            setPSet(E, derefPSet(getPSet(E->getSubExpr())));
            return;
        case CK_NullToPointer:
            setPSet(E, PSet::null(NullReason::nullptrConstant(E->getSourceRange(), CurrentBlock)));
            return;
        case CK_LValueToRValue:
            // For L-values, the pset refers to the memory location,
            // which in turn points to the pointee. For R-values,
            // the pset just refers to the pointee.
            if (hasPSet(E)) {
                setPSet(E, derefPSet(getPSet(E->getSubExpr())));
            }
            return;
        default:
            llvm_unreachable("Should have been ignored in IgnoreTransparentExprs()");
            return;
        }
    }

    /// Returns true if all of the following is true
    /// 1) BO is an addition,
    /// 2) LHS is ImplicitCastExpr <ArrayToPointerDecay> of DeclRefExpr of array
    /// type 3) RHS is IntegerLiteral 4) The value of the IntegerLiteral is
    /// between 0 and the size of the array type
    static bool isArrayPlusIndex(const BinaryOperator* BO)
    {
        if (BO->getOpcode() != BO_Add) {
            return false;
        }

        auto* ImplCast = dyn_cast<ImplicitCastExpr>(BO->getLHS());
        if (!ImplCast) {
            return false;
        }

        if (ImplCast->getCastKind() != CK_ArrayToPointerDecay) {
            return false;
        }
        auto* DeclRef = dyn_cast<DeclRefExpr>(ImplCast->getSubExpr());
        if (!DeclRef) {
            return false;
        }
        const auto* ArrayType = dyn_cast_or_null<ConstantArrayType>(DeclRef->getType()->getAsArrayTypeUnsafe());
        if (!ArrayType) {
            return false;
        }
        llvm::APInt ArrayBound = ArrayType->getSize();

        auto* Integer = dyn_cast<IntegerLiteral>(BO->getRHS());
        if (!Integer) {
            return false;
        }

        llvm::APInt Offset = Integer->getValue();
        if (Offset.isNegative()) {
            return false;
        }

        // ule() comparison requires both APInt to have the same bit width.
        if (ArrayBound.getBitWidth() > Offset.getBitWidth()) {
            Offset = Offset.zext(ArrayBound.getBitWidth());
        } else if (ArrayBound.getBitWidth() < Offset.getBitWidth()) {
            ArrayBound = ArrayBound.zext(Offset.getBitWidth());
        }

        // We explicitly allow to form the pointer one element after the array
        // to support the compiler-generated code for the end iterator in a
        // for-range loop.
        // TODO: this allows to form an invalid pointer, where we would not detect
        // dereferencing.
        return Offset.ule(ArrayBound);
    }

    void VisitBinaryOperator(const BinaryOperator* BO)
    {
        if (BO->getOpcode() == BO_Assign) {
            // Owners usually are user defined types. We should see a function call.
            // Do we need to handle raw pointers annotated as owners?
            if (isPointer(BO)) {
                // This assignment updates a Pointer.
                const SourceRange Range = BO->getRHS()->getSourceRange();
                const PSet LHS = handlePointerAssign(BO->getLHS()->getType(), getPSet(BO->getRHS()), Range);
                setPSet(getPSet(BO->getLHS()), LHS, Range);
            }

            setPSet(BO, getPSet(BO->getLHS()));
        } else if (isArrayPlusIndex(BO)) { // NOLINT(bugprone-branch-clone)
            setPSet(BO, getPSet(BO->getLHS()));
        } else if (BO->getType()->isPointerType()) {
            Reporter.warnPointerArithmetic(BO->getOperatorLoc());
            AnalysisDisabled = true;
            setPSet(BO, {});
        } else if (BO->isLValue() && BO->isCompoundAssignmentOp()) {
            setPSet(BO, getPSet(BO->getLHS()));
        } else if (BO->isLValue() && BO->getOpcode() == BO_Comma) {
            setPSet(BO, getPSet(BO->getRHS()));
        } else if (BO->getOpcode() == BO_PtrMemD || BO->getOpcode() == BO_PtrMemI) {
            setPSet(BO, {}); // TODO, not specified in paper
        }
    }

    void VisitUnaryOperator(const UnaryOperator* UO)
    {
        switch (UO->getOpcode()) {
        case UO_AddrOf:
            break;
        case UO_Deref: {
            auto PS = getPSet(UO->getSubExpr());
            checkPSetValidity(PS, UO->getSourceRange());
            setPSet(UO, PS);
            return;
        }
        default:
            // Workaround: detecting compiler generated AST node.
            if (isPointer(UO) && UO->getBeginLoc() != UO->getEndLoc()) {
                Reporter.warnPointerArithmetic(UO->getOperatorLoc());
                AnalysisDisabled = true;
                setPSet(getPSet(UO->getSubExpr()), {}, UO->getSourceRange());
            }
        }

        if (hasPSet(UO)) {
            PSet PS = getPSet(UO->getSubExpr());
            if (!UO->isLValue() && isa<MemberExpr>(UO->getSubExpr())) {
                PS = derefPSetForMemberIfNecessary(PS);
            }
            setPSet(UO, PS);
        }
    }

    void VisitReturnStmt(const ReturnStmt* R)
    {
        const Expr* RetVal = R->getRetValue();
        if (!RetVal) {
            return;
        }

        if (!isPointer(RetVal) && !RetVal->isLValue()) {
            return;
        }

        auto RetPSet = getPSet(RetVal);
        if (RetVal->isLValue() && isa<MemberExpr>(RetVal)) {
            RetPSet = derefPSetForMemberIfNecessary(RetPSet);
        }

        // TODO: Would be nicer if the LifetimeEnds CFG nodes would appear before
        // the ReturnStmt node
        for (const auto& Var : RetPSet.vars()) {
            if (Var.isTemporary()) {
                RetPSet = PSet::invalid(InvalidationReason::TemporaryLeftScope(R->getSourceRange(), CurrentBlock));
                break;
            }

            if (const auto* VD = Var.asVarDecl()) {
                // Allow to return a pointer to *p (then p is a parameter).
                if (VD->hasLocalStorage()
                    && (!Var.isDeref() || classifyTypeCategory(VD->getType()) == TypeCategory::Owner)) {
                    RetPSet
                        = PSet::invalid(InvalidationReason::PointeeLeftScope(R->getSourceRange(), CurrentBlock, VD));
                    break;
                }
            }
        }
        PSetsMap PostConditions;
        getLifetimeContracts(PostConditions, AnalyzedFD, ASTCtxt, CurrentBlock, IsConvertible, Reporter, /*Pre=*/false);
        RetPSet.checkSubstitutableFor(
            PostConditions[Variable::returnVal()], R->getSourceRange(), Reporter, ValueSource::Return);
    }

    void VisitCXXConstructExpr(const CXXConstructExpr* E)
    {
        if (!isPointer(E)) {
            // Default, will be overwritten if it makes sense.
            setPSet(E, {});

            // Constructing a temporary owner/value
            CallVisitor C(*this, Reporter, CurrentBlock);
            C.enforcePreAndPostConditions(E, ASTCtxt, IsConvertible);

            return;
        }

        if (E->getNumArgs() == 0) {
            setPSet(E, PSet::null(NullReason::defaultConstructed(E->getSourceRange(), CurrentBlock)));
            return;
        }

        auto* Ctor = E->getConstructor();
        auto ParmTy = Ctor->getParamDecl(0)->getType();
        auto TC = classifyTypeCategory(E->getArg(0)->getType());
        // For ctors taking a const reference we assume that we will not take the
        // address of the argument but copy it.
        // TODO: Use the function call rules here.
        if (TC == TypeCategory::Owner || Ctor->isCopyOrMoveConstructor()
            || (ParmTy->isReferenceType() && ParmTy->getPointeeType().isConstQualified())) {
            setPSet(E, derefPSet(getPSet(E->getArg(0))));
        } else if (TC == TypeCategory::Pointer || isa<CXXNullPtrLiteralExpr>(E->getArg(0))) {
            setPSet(E, getPSet(E->getArg(0)));
        } else {
            setPSet(E, PSet::invalid(InvalidationReason::NotInitialized(E->getSourceRange(), CurrentBlock)));
        }
    }

    void VisitCXXStdInitializerListExpr(const CXXStdInitializerListExpr* E)
    {
        if (hasPSet(E)) {
            setPSet(E, getPSet(E->getSubExpr()));
        }
    }

    void VisitCXXDefaultArgExpr(const CXXDefaultArgExpr* E)
    {
        if (hasPSet(E)) {
            // FIXME: We should do setPSet(E, getPSet(E->getSubExpr())),
            // but the getSubExpr() is not visited as part of the CFG,
            // so it does not have a pset.
            setPSet(E, PSet::globalVar(false));
        }
    }

    void VisitImplicitValueInitExpr(const ImplicitValueInitExpr* E)
    {
        // We don't really care, because this expression is not referenced
        // anywhere. But still set it to satisfy the VisitStmt() post-condition.
        setPSet(E, {});
    }

    void VisitCompoundLiteralExpr(const CompoundLiteralExpr* E)
    {
        // C99 construct. We ignore it, but still set the pset to satisfy the
        // VisitStmt() post-condition.
        setPSet(E, {});
    }

    void VisitVAArgExpr(const VAArgExpr* E) { setPSet(E, {}); }

    void VisitStmtExpr(const StmtExpr* E)
    {
        // TODO: not yet suppported.
        setPSet(E, {});
    }

    void VisitCXXDeleteExpr(const CXXDeleteExpr* DE)
    {
        if (hasPSet(DE->getArgument())) {
            const PSet PS = getPSet(DE->getArgument());
            for (const auto& Var : PS.vars()) {
                // TODO: diagnose if we are deleting the buffer of on owner?
                invalidateVar(Var, InvalidationReason::Deleted(DE->getSourceRange(), CurrentBlock));
            }
        }
    }

    void VisitCXXThrowExpr(const CXXThrowExpr* TE)
    {
        if (!TE->getSubExpr()) {
            return;
        }
        if (!isPointer(TE->getSubExpr())) {
            return;
        }
        const PSet ThrownPSet = getPSet(TE->getSubExpr());
        if (!ThrownPSet.isGlobal()) {
            Reporter.warnNonStaticThrow(TE->getSourceRange(), ThrownPSet.str());
        }
    }

    /// Evaluates the CallExpr for effects on psets.
    /// When a non-const pointer to pointer or reference to pointer is passed
    /// into a function, it's pointee's are invalidated.
    /// Returns true if CallExpr was handled.
    void VisitCallExpr(const CallExpr* CallE)
    {
        if (handleDebugFunctions(CallE)) {
            return;
        }

        CallVisitor C(*this, Reporter, CurrentBlock);
        C.run(CallE, ASTCtxt, IsConvertible);
    }

    bool checkPSetValidity(const PSet& PS, SourceRange Range) const;

    void invalidateVar(const Variable& V, const InvalidationReason& Reason) override
    {
        for (const auto& I : PMap) {
            const PSet& PS = I.second;
            if (PS.containsInvalid()) {
                continue; // Nothing to invalidate
            }

            const auto& Var = I.first;
            if (PS.containsParent(V)) {
                setPSet(PSet::singleton(Var), PSet::invalid(Reason), Reason.getRange());
            }
        }
    }

    void invalidateOwner(const Variable& V, const InvalidationReason& Reason) override
    {
        for (const auto& I : PMap) {
            const auto& Var = I.first;
            if (V == Var) {
                continue; // Invalidating Owner' should not change the pset of Owner
            }
            const PSet& PS = I.second;
            if (PS.containsInvalid()) {
                continue; // Nothing to invalidate
            }

            auto DerefV = V;
            DerefV.deref();
            if (PS.containsParent(DerefV)) {
                setPSet(PSet::singleton(Var), PSet::invalid(Reason), Reason.getRange());
            }
        }
    }

    // Remove the variable from the pset together with the materialized
    // temporaries extended by that variable. It also invalidates the pointers
    // pointing to these.
    // If VD is nullptr, invalidates all psets that contain
    // MaterializeTemporaryExpr without extending decl.
    void eraseVariable(const VarDecl* VD, SourceRange Range)
    {
        const InvalidationReason Reason = VD ? InvalidationReason::PointeeLeftScope(Range, CurrentBlock, VD)
                                             : InvalidationReason::TemporaryLeftScope(Range, CurrentBlock);
        if (VD) {
            PMap.erase(VD);
            invalidateVar(VD, Reason);
        }
        // Remove all materialized temporaries that were extended by this
        // variable (or a lifetime extended temporary without an extending
        // declaration) and do the invalidation.
        for (auto I = PMap.begin(); I != PMap.end();) {
            if (I->first.isTemporaryExtendedBy(VD)) {
                I = PMap.erase(I);
            } else {
                const auto& Var = I->first;
                auto& Pset = I->second;
                const bool PsetContainsTemporary
                    = llvm::any_of(Pset.vars(), [VD](const Variable& V) { return V.isTemporaryExtendedBy(VD); });
                if (PsetContainsTemporary) {
                    setPSet(PSet::singleton(Var), PSet::invalid(Reason), Reason.getRange());
                }
                ++I;
            }
        }
    }

    PSet getPSet(const Variable& P) const override;

    PSet getPSet(const Expr* E, bool AllowNonExisting = false) const override
    {
        E = ignoreTransparentExprs(E);
        if (E->isLValue()) {
            auto I = RefersTo.find(E);
            if (I != RefersTo.end()) {
                return I->second;
            }
            if (AllowNonExisting) {
                return {};
            }
#ifndef NDEBUG
            E->dump();
            llvm_unreachable("Expression has no entry in RefersTo");
#endif
            return {};
        } else { // NOLINT(readability-else-after-return)
            if (isa<CXXNullPtrLiteralExpr>(E)) {
                return PSet::null(NullReason::nullptrConstant(E->getSourceRange(), CurrentBlock));
            }

            auto I = PSetsOfExpr.find(E);
            if (I != PSetsOfExpr.end()) {
                return I->second;
            }
            if (AllowNonExisting) {
                return {};
            }
#ifndef NDEBUG
            E->dump();
            llvm_unreachable("Expression has no entry in PSetsOfExpr");
#endif
            return {};
        }
    }

    PSet getPSet(const PSet& P) const override
    {
        PSet Ret;
        if (P.containsInvalid()) {
            return PSet::invalid(P.invReasons());
        }

        for (const auto& Var : P.vars()) {
            Ret.merge(getPSet(Var));
        }

        if (P.containsGlobal()) {
            Ret.merge(PSet::globalVar(false));
        }
        return Ret;
    }

    void setPSet(const Expr* E, const PSet& PS) override
    {
        if (E->isLValue()) {
            DBG("Set RefersTo[" << E->getStmtClassName() << "] = " << PS.str() << "\n");
            RefersTo[E] = PS;
        } else {
            DBG("Set PSetsOfExpr[" << E->getStmtClassName() << "] = " << PS.str() << "\n");
            PSetsOfExpr[E] = PS;
        }
    }
    void setPSet(const PSet& LHS, PSet RHS, SourceRange Range) override;

    PSet derefPSet(const PSet& P) const override;
    PSet derefPSetForMemberIfNecessary(const PSet& P) const;

    bool handleDebugFunctions(const CallExpr* CallE) const;

    void debugPmap(SourceRange Range) const override;

    PSet handlePointerAssign(QualType LHS, PSet RHS, SourceRange Range, bool AddReason = true) const override
    {
        if (RHS.containsNull()) {
            if (AddReason) {
                RHS.addNullReason(NullReason::assigned(Range, CurrentBlock));
            }
            if (!isNullableType(LHS)) {
                Reporter.warn(WarnType::AssignNull, Range, !RHS.isNull());
                RHS = PSet {};
            }
        }
        return RHS;
    }

    // NOLINTNEXTLINE(readability-identifier-naming): required by parent
    void VisitVarDecl(const VarDecl* VD)
    {
        // TODO: handle DecompositionDecl.
        const Expr* Initializer = VD->getInit();
        const SourceRange Range = VD->getSourceRange();

        switch (classifyTypeCategory(VD->getType())) {
        case TypeCategory::Pointer: {
            PSet PS;
            if (Initializer) {
                // For raw pointers, show here the assignment. For other Pointers,
                // we will have seen a CXXConstructor, which added a NullReason.
                PS = handlePointerAssign(
                    VD->getType(), getPSet(Initializer), VD->getSourceRange(), VD->getType()->isPointerType());
            } else if (VD->hasGlobalStorage()) {
                // Never treat local statics as uninitialized.
                PS = PSet::globalVar(/*TODO*/ false);
            } else {
                PS = PSet::invalid(InvalidationReason::NotInitialized(VD->getLocation(), CurrentBlock));
            }
            setPSet(PSet::singleton(VD), PS, Range);
            break;
        }
        case TypeCategory::Owner: {
            setPSet(PSet::singleton(VD), PSet::singleton(VD, 1), Range);
            break;
        }
        default:
            // TODO: now for all non-Pointer, set pset(v) = {v}
            setPSet(PSet::singleton(VD), PSet::singleton(VD), Range);
            break;
        }
    }

    void VisitLambdaExpr(const LambdaExpr* L)
    {
        PSet PS;

        for (const auto V : L->captures()) {
            if (V.capturesThis()) {
                const auto* RD = cast<CXXMethodDecl>(AnalyzedFD)->getParent();
                PS.merge(PSet::singleton(Variable::thisPointer(RD), 1));
                continue;
            }
            if (V.capturesVariable()) {
                const auto* VD = V.getCapturedVar()->getPotentiallyDecomposedVarDecl();
                if (!VD) {
                    continue;
                }

                if (V.getCaptureKind() == LCK_ByRef) {
                    if (classifyTypeCategory(VD->getType()).isIndirection()) {
                        PS.merge(getPSet(VD));
                    } else {
                        PS.merge(PSet::singleton(VD));
                    }
                    continue;
                }

                if (classifyTypeCategory(VD->getType()).isPointer()) {
                    PS.merge(getPSet(VD));
                }
            }
        }

        if (PS.isUnknown()) {
            PS.addGlobal();
        }

        setPSet(L, PS);
    }

    void updatePSetsFromCondition(
        const Stmt* S, bool Positive, std::optional<PSetsMap>& FalseBranchExitPMap, SourceRange Range);

public:
    PSetsBuilder(const FunctionDecl* FD, LifetimeReporterBase& Reporter, ASTContext& ASTCtxt, PSetsMap& PMap,
        std::map<const Expr*, PSet>& PSetsOfExpr, std::map<const Expr*, PSet>& RefersTo, IsConvertibleTy IsConvertible)
        : AnalyzedFD(FD)
        , Reporter(Reporter)
        , ASTCtxt(ASTCtxt)
        , IsConvertible(IsConvertible)
        , PMap(PMap)
        , PSetsOfExpr(PSetsOfExpr)
        , RefersTo(RefersTo)
    {
    }

    ~PSetsBuilder() override = default;

    DISALLOW_COPY_AND_MOVE(PSetsBuilder);

    void visitBlock(const CFGBlock& B, std::optional<PSetsMap>& FalseBranchExitPMap);
}; // namespace
} // namespace

// Manages lifetime information for the CFG of a FunctionDecl
PSet PSetsBuilder::getPSet(const Variable& P) const
{
    // Assumption: global Pointers have a pset of {static}
    if (P.hasStaticLifetime()) {
        return PSet::globalVar(false);
    }

    auto I = PMap.find(P);
    if (I != PMap.end()) {
        return I->second;
    }

    // Assume that the unseen pointer fields are valid. We will always have
    // unseen fields since we do not track the fields of owners and values.
    // Until proper aggregate support is implemented, this might be triggered
    // unintentionally.
    if (P.isField()) {
        return PSet::globalVar(false);
    }

    // consider: ***v
    if (P.getType().isNull()) {
        if (P.getDerefNum() > MaxOrderDepth) {
            return PSet::globalVar(false);
        }

        return PSet::singleton(P, 1);
    }

    if (P.getType()->isArrayType()) {
        // This triggers when we have an array of Pointers, and we
        // do a subscript to obtain a Pointer.
        // TODO: We currently have no idea what that Pointer may point at.
        // The array itself should have a pset. It starts with (invalid) if there
        // is not initialization done. Whenever there is an assignment to any array
        // member, the pset of the array should grow by the pset of the assigment
        // LHS. (This means that an uninitialized array can never become valid. This
        // is reasonable, because we have no way to track if all array elements have
        // been set at some later point.)
        return {};
    }

    if (const auto* VD = P.asVarDecl()) {
        // Handle goto_forward_over_decl() in test attr-pset.cpp
        if (!isa<ParmVarDecl>(VD)) {
            return PSet::invalid(InvalidationReason::NotInitialized(VD->getLocation(), CurrentBlock));
        }
    }

    return {};
}

/// Computes the pset of dereferencing a variable with the given pset
/// If PS contains (null), it is silently ignored.
PSet PSetsBuilder::derefPSet(const PSet& PS) const
{
    // When a local Pointer p is dereferenced using unary * or -> to create a
    // temporary tmp, then if pset(pset(p)) is nonempty, set pset(tmp) =
    // pset(pset(p)) and Kill(pset(tmp)'). Otherwise, set pset(tmp) = {tmp}.
    if (PS.isUnknown()) {
        return {};
    }

    if (PS.containsInvalid()) {
        return {}; // Return unknown, so we don't diagnose again.
    }

    PSet RetPS;
    if (PS.containsGlobal()) {
        RetPS.addGlobal();
    }

    for (auto V : PS.vars()) {
        const unsigned Order = V.getOrder();
        if (Order > 0) {
            if (Order > MaxOrderDepth) {
                RetPS.addGlobal();
            } else {
                RetPS.insert(V.deref()); // pset(o') = { o'' }
            }
        } else {
            RetPS.merge(getPSet(V));
        }
    }

    return RetPS;
}

PSet PSetsBuilder::derefPSetForMemberIfNecessary(const PSet& PS) const
{
    if (PS.isUnknown()) {
        return {};
    }

    if (PS.containsInvalid()) {
        return PS;
    }

    PSet RetPS;
    if (PS.containsGlobal()) {
        RetPS.addGlobal();
    }

    for (const auto& V : PS.vars()) {
        const auto It = PMap.find(V);
        if (It != PMap.end()) {
            RetPS.merge(It->second);
        } else {
            RetPS.merge(PSet::singleton(V));
        }
    }

    return RetPS;
}

void PSetsBuilder::setPSet(const PSet& LHS, PSet RHS, SourceRange Range)
{
    // Assumption: global Pointers have a pset that is a subset of {static,
    // null}
    if (LHS.isGlobal() && !RHS.isUnknown() && !RHS.isGlobal() && !RHS.isNull()) {
        const StringRef SourceText = Lexer::getSourceText(
            CharSourceRange::getTokenRange(Range), ASTCtxt.getSourceManager(), ASTCtxt.getLangOpts());
        Reporter.warnPsetOfGlobal(Range, SourceText, RHS.str());
    }

    DBG("PMap[" << LHS.str() << "] = " << RHS.str() << "\n");
    if (LHS.vars().size() == 1) {
        const Variable Var = *LHS.vars().begin();
        RHS.addReasonTarget(Var);
        auto I = PMap.find(Var);
        if (I != PMap.end()) {
            I->second = std::move(RHS);
        } else {
            PMap.emplace(Var, RHS);
        }
    } else {
        for (const auto& V : LHS.vars()) {
            auto I = PMap.find(V);
            if (I != PMap.end()) {
                I->second.merge(RHS);
            } else {
                PMap.emplace(V, RHS);
            }
        }
    }
}

bool PSetsBuilder::checkPSetValidity(const PSet& PS, SourceRange Range) const
{
    if (PS.containsInvalid()) {
        if (PS.shouldBeFilteredBasedOnNotes(Reporter) && !PS.isInvalid()) {
            return false;
        }
        Reporter.warn(WarnType::DerefDangling, Range, !PS.isInvalid());
        PS.explainWhyInvalid(Reporter);
        return false;
    }

    if (PS.containsNull()) {
        if (PS.shouldBeFilteredBasedOnNotes(Reporter) && !PS.isNull()) {
            return false;
        }
        Reporter.warn(WarnType::DerefNull, Range, !PS.isNull());
        PS.explainWhyNull(Reporter);
        return false;
    }
    return true;
}

/// Updates psets to remove 'null' when entering conditional statements. If
/// 'positive' is false, handles expression as-if it was negated.
/// Examples:
///   int* p = f();
/// if(p)
///  ... // pset of p does not contain 'null'
/// else
///  ... // pset of p is 'null'
/// if(!p)
///  ... // pset of p is 'null'
/// else
///  ... // pset of p does not contain 'null'
// NOLINTNEXTLINE(readability-function-cognitive-complexity): legacy code
void PSetsBuilder::updatePSetsFromCondition(
    const Stmt* S, bool Positive, std::optional<PSetsMap>& FalseBranchExitPMap, SourceRange Range)
{
    const auto* E = dyn_cast_or_null<Expr>(S);
    if (!E) {
        return;
    }
    E = ignoreTransparentExprs(E, /*IgnoreLValueToRValue=*/true);
    // Handle user written bool conversion.
    if (const auto* CE = dyn_cast<CXXMemberCallExpr>(E)) {
        if (const auto* ConvDecl = dyn_cast_or_null<CXXConversionDecl>(CE->getDirectCallee())) {
            if (ConvDecl->getConversionType()->isBooleanType()) {
                updatePSetsFromCondition(
                    CE->getImplicitObjectArgument(), Positive, FalseBranchExitPMap, E->getSourceRange());
            }
        }
        return;
    }
    if (const auto* UO = dyn_cast<UnaryOperator>(E)) {
        if (UO->getOpcode() != UO_LNot) {
            return;
        }
        E = UO->getSubExpr();
        updatePSetsFromCondition(E, !Positive, FalseBranchExitPMap, E->getSourceRange());
        return;
    }
    if (const auto* BO = dyn_cast<BinaryOperator>(E)) {
        const BinaryOperator::Opcode OC = BO->getOpcode();
        if (OC != BO_NE && OC != BO_EQ) {
            return;
        }
        // The p == null is the negative case.
        if (OC == BO_EQ) {
            Positive = !Positive;
        }
        const auto* LHS = ignoreTransparentExprs(BO->getLHS());
        const auto* RHS = ignoreTransparentExprs(BO->getRHS());
        if (!isPointer(LHS) || !isPointer(RHS)) {
            return;
        }

        if (getPSet(RHS).isNull()) {
            updatePSetsFromCondition(LHS, Positive, FalseBranchExitPMap, E->getSourceRange());
        } else if (getPSet(LHS).isNull()) {
            updatePSetsFromCondition(RHS, Positive, FalseBranchExitPMap, E->getSourceRange());
        }
        return;
    }

    auto TC = classifyTypeCategory(E->getType());
    if (E->isLValue() && (TC == TypeCategory::Pointer || TC == TypeCategory::Owner)) {
        auto Ref = getPSet(E);
        // We refer to multiple variables (or none),
        // and we cannot know which of them is null/non-null.
        if (Ref.vars().size() != 1) {
            return;
        }

        const Variable V = *Ref.vars().begin();
        Variable DerefV = V;
        DerefV.deref();
        PSet PS = getPSet(V);
        PSet PSElseBranch = PS;
        FalseBranchExitPMap = PMap;
        if (Positive) {
            // The variable is non-null in the if-branch and null in the then-branch.
            PSElseBranch.removeEverythingButNull();
            PS.removeNull();
            auto It = FalseBranchExitPMap->find(DerefV);
            if (It != FalseBranchExitPMap->end()) {
                It->second = PSet();
            }
        } else {
            // The variable is null in the if-branch and non-null in the then-branch.
            PS.removeEverythingButNull();
            PSElseBranch.removeNull();
            auto It = PMap.find(DerefV);
            if (It != PMap.end()) {
                It->second = PSet();
            }
        }
        (*FalseBranchExitPMap)[V] = PSElseBranch;
        setPSet(PSet::singleton(V), PS, Range);
    }
} // namespace lifetime

static const FunctionDecl* getFunctionDecl(const Expr* E)
{
    if (const auto* Lambda = dyn_cast<LambdaExpr>(E)) {
        const auto* CE = dyn_cast<CallExpr>(Lambda->getCompoundStmtBody()->body_back());
        if (!CE) {
            return nullptr;
        }

        return CE->getDirectCallee()->getCanonicalDecl();
    }

    const auto* FD = std::invoke([E]() -> const FunctionDecl* {
        if (const auto* CE = dyn_cast<CallExpr>(E)) {
            return CE->getDirectCallee();
        }
        if (const auto* Tmp = dyn_cast<CXXTemporaryObjectExpr>(E)) {
            return Tmp->getConstructor();
        }
        if (const auto* R = dyn_cast<DeclRefExpr>(E)) {
            return dyn_cast<FunctionDecl>(R->getDecl());
        }

        return nullptr;
    });

    if (FD) {
        FD = FD->getCanonicalDecl();
    }

    return FD;
}

/// Checks if the statement S is a call to a debug function and dumps the
/// corresponding part of the state.
bool PSetsBuilder::handleDebugFunctions(const CallExpr* CallE) const
{
    const FunctionDecl* Callee = CallE->getDirectCallee();
    if (!Callee) {
        return false;
    }

    const auto* I = Callee->getIdentifier();
    if (!I) {
        return false;
    }

    const auto FuncNum = llvm::StringSwitch<int>(I->getName())
                             .Case("__lifetime_pset", 1)
                             .Case("__lifetime_pset_ref", 2)
                             .Case("__lifetime_type_category", 3)
                             .Case("__lifetime_type_category_arg", 4)
                             .Case("__lifetime_contracts", 5)
                             .Case("__lifetime_pmap", 6)
                             .Default(0);

    const auto Range = CallE->getSourceRange();
    switch (FuncNum) {
    case 1: // __lifetime_pset
    case 2: { // __lifetime_pset_ref
        assert(CallE->getNumArgs() == 1 && "__lifetime_pset takes one argument");
        PSet Set = getPSet(CallE->getArg(0));

        if (FuncNum == 1) {
            if (!hasPSet(CallE->getArg(0))) {
                return true; // Argument must be a Pointer or Owner
            }
            Set = getPSet(Set);
        }
        const StringRef SourceText
            = Lexer::getSourceText(CharSourceRange::getTokenRange(CallE->getArg(0)->getSourceRange()),
                ASTCtxt.getSourceManager(), ASTCtxt.getLangOpts());
        Reporter.debugPset(Range, SourceText, Set.str());
        return true;
    }
    case 3: { // __lifetime_type_category
        const auto* Args = Callee->getTemplateSpecializationArgs();
        auto QType = Args->get(0).getAsType();
        auto Class = classifyTypeCategory(QType);
        if (Class.isIndirection()) {
            Reporter.debugTypeCategory(Range, Class.TC, Class.PointeeType.getAsString());
        } else {
            Reporter.debugTypeCategory(Range, Class.TC);
        }
        return true;
    }
    case 4: { // __lifetime_type_category_arg
        auto QType = CallE->getArg(0)->getType();
        auto Class = classifyTypeCategory(QType);
        if (Class.isIndirection()) {
            Reporter.debugTypeCategory(Range, Class.TC, Class.PointeeType.getAsString());
        } else {
            Reporter.debugTypeCategory(Range, Class.TC);
        }
        return true;
    }
    case 5: { // __lifetime_contracts
        const Expr* E = CallE->getArg(0)->IgnoreImplicit();
        if (const auto* UO = dyn_cast<UnaryOperator>(E)) {
            E = UO->getSubExpr();
        }
        const auto* FD = getFunctionDecl(E);
        if (FD == nullptr) {
            return false;
        }

        PSetsMap PreConditions;
        getLifetimeContracts(PreConditions, FD, ASTCtxt, CurrentBlock, IsConvertible, Reporter, /*Pre=*/true);

        PSetsMap PostConditions;
        getLifetimeContracts(PostConditions, FD, ASTCtxt, CurrentBlock, IsConvertible, Reporter, /*Pre=*/false);

        auto PrintContract = [this, &Range](const auto& E, const std::string& Contract) {
            std::string KeyText = Contract + "(";
            KeyText += E.first.getName();
            KeyText += ")";
            const std::string PSetText = E.second.str();
            Reporter.debugPset(Range, KeyText, PSetText);
        };
        for (const auto& E : PreConditions) {
            PrintContract(E, "Pre");
        }
        for (const auto& E : PostConditions) {
            PrintContract(E, "Post");
        }

        return true;
    }
    case 6:
        debugPmap(Range);
        return true;

    default:
        return false;
    }
}

void PSetsBuilder::debugPmap(const SourceRange) const
{
    llvm::errs() << "---\n";
    llvm::errs() << "PMap\n";
    for (const auto& [V, P] : PMap) {
        llvm::errs() << "pset(" << V.getName() << ") -> " << P.str() << "\n";
    }

    llvm::errs() << "\nRefersTo\n";
    for (const auto& [V, P] : RefersTo) {
        llvm::errs() << "pset(";
        V->dumpPretty(ASTCtxt);
        llvm::errs() << ") -> " << P.str() << "\n";
    }

    llvm::errs() << "\nPSetsOfExpr\n";
    for (const auto& [V, P] : PSetsOfExpr) {
        llvm::errs() << "pset(";
        V->dumpPretty(ASTCtxt);
        llvm::errs() << ") -> " << P.str() << "\n";
    }
}

static const Stmt* getRealTerminator(const CFGBlock& B)
{
    // For expressions like (bool)(p && q), q will only have one successor,
    // the cast operation. But we still want to compute two sets for q so
    // we can propagate this information through the cast.
    if (B.succ_size() == 1 && !B.empty() && B.rbegin()->getKind() == CFGElement::Kind::Statement) {
        auto* Succ = B.succ_begin()->getReachableBlock();
        if (Succ && isNoopBlock(*Succ) && Succ->succ_size() == 2) {
            return B.rbegin()->castAs<CFGStmt>().getStmt();
        }
    }

    return B.getLastCondition();
}

static bool isThrowingBlock(const CFGBlock& B)
{
    return llvm::any_of(B, [](const CFGElement& E) {
        return E.getKind() == CFGElement::Statement && isa<CXXThrowExpr>(E.getAs<CFGStmt>()->getStmt());
    });
}

static SourceRange getSourceRange(const CFGElement& E)
{
    if (auto S = E.getAs<CFGStmt>()) {
        return S->getStmt()->getSourceRange();
    }
    if (auto S = E.getAs<CFGLifetimeEnds>()) {
        return S->getTriggerStmt()->getSourceRange();
    }
    return {};
}

// Update PSets in Builder through all CFGElements of this block
// NOLINTNEXTLINE(readability-function-cognitive-complexity): refine this later
void PSetsBuilder::visitBlock(const CFGBlock& B, std::optional<PSetsMap>& FalseBranchExitPMap)
{
    CurrentBlock = &B;
    for (const auto& E : B) {
        switch (E.getKind()) {
        case CFGElement::Statement: {
            const Stmt* S = E.castAs<CFGStmt>().getStmt();

            // ignore codes that have bugs
            if (isa<RecoveryExpr>(S) || isa<UnresolvedLookupExpr>(S) || isa<UnresolvedMemberExpr>(S)) {
                return;
            }

            if (!isIgnoredStmt(S)) {
                Visit(S);
                if (isAnalysisDisabled()) {
                    return;
                }
#ifndef NDEBUG
                if (const auto* Ex = dyn_cast<Expr>(S)) {
                    if (Ex->isLValue() && !Ex->getType()->isFunctionType() && RefersTo.find(Ex) == RefersTo.end()) {
                        Ex->dump();
                        llvm_unreachable("Missing entry in RefersTo");
                    }
                    if (!Ex->isLValue() && hasPSet(Ex) && PSetsOfExpr.find(Ex) == PSetsOfExpr.end()) {
                        Ex->dump();
                        llvm_unreachable("Missing entry in PSetsOfExpr");
                    }
                }
#endif
            }

            // Kill all temporaries that vanish at the end of the full expression
            if (isa<DeclStmt>(S)) {
                // Remove all materialized temporaries that are not extended.
                eraseVariable(nullptr, S->getEndLoc());
            }
            if (!isa<Expr>(S)) {
                // Clean up P- and RefersTo-sets for subexpressions.
                // We should never reference subexpressions again after
                // the full expression ended. The problem is,
                // it is not trivial to find out the end of a full
                // expression with linearized CFGs.
                // This is why currently the sets are only cleared for
                // statements which are not expressions.
                // TODO: clean this up by properly tracking end of full exprs.
                RefersTo.clear();
                PSetsOfExpr.clear();
            }

            break;
        }
        case CFGElement::LifetimeEnds: {
            auto Leaver = E.castAs<CFGLifetimeEnds>();
            const auto* VD = Leaver.getVarDecl();
            if (const auto* RD = VD->getType()->getAsCXXRecordDecl()) {
                if (const auto* Dtor = RD->getDestructor()) {
                    if (isAnnotatedWith(Dtor, LifetimePre)) {
                        checkPSetValidity(getPSet(VD), Leaver.getTriggerStmt()->getEndLoc());
                    }
                }
            }

            // Stop tracking Variables that leave scope.
            eraseVariable(VD, Leaver.getTriggerStmt()->getEndLoc());
            break;
        }
        case CFGElement::NewAllocator:
        case CFGElement::AutomaticObjectDtor:
        case CFGElement::DeleteDtor:
        case CFGElement::BaseDtor:
        case CFGElement::MemberDtor:
            break;

        case CFGElement::TemporaryDtor: {
            // Acts as ExprWithCleanups
            auto Dtor = E.castAs<CFGTemporaryDtor>();
            eraseVariable(nullptr, Dtor.getBindTemporaryExpr()->getEndLoc());
            break;
        }

        case CFGElement::Initializer:
        case CFGElement::ScopeBegin:
        case CFGElement::ScopeEnd:
        case CFGElement::LoopExit:
        case CFGElement::Constructor: // TODO
        case CFGElement::CXXRecordTypedCall:
            break;
        }
    }
    if (const auto* Terminator = getRealTerminator(B)) {
        updatePSetsFromCondition(Terminator, /*Positive=*/true, FalseBranchExitPMap, Terminator->getEndLoc());
    }
    if (B.hasNoReturnElement() || isThrowingBlock(B)) {
        return;
    }
    if (B.succ_size() == 1 && *B.succ_begin() == &B.getParent()->getExit()) {
        PSetsMap PostConditions;
        getLifetimeContracts(PostConditions, AnalyzedFD, ASTCtxt, &B, IsConvertible, Reporter, /*Pre=*/false);
        for (auto& VarToPSet : PostConditions) {
            if (VarToPSet.first.isReturnVal()) {
                continue;
            }
            auto OutVarIt = PMap.find(VarToPSet.first);
            assert(OutVarIt != PMap.end());

            // HACK: output variable kept invalid on error path
            if (OutVarIt->second.containsInvalid() && !Reporter.getOptions().LifetimeOutput) {
                OutVarIt->second.removeEverythingButNull();
            }

            OutVarIt->second.checkSubstitutableFor(VarToPSet.second, getSourceRange(B.back()), Reporter,
                ValueSource::OutputParam, VarToPSet.first.getName());
        }
    }
} // namespace lifetime

bool VisitBlock(const FunctionDecl* FD, PSetsMap& PMap, std::optional<PSetsMap>& FalseBranchExitPMap,
    std::map<const Expr*, PSet>& PSetsOfExpr, std::map<const Expr*, PSet>& RefersTo, const CFGBlock& B,
    LifetimeReporterBase& Reporter, ASTContext& ASTCtxt, IsConvertibleTy IsConvertible)
{
    Reporter.setCurrentBlock(&B);
    PSetsBuilder Builder(FD, Reporter, ASTCtxt, PMap, PSetsOfExpr, RefersTo, IsConvertible);
    Builder.visitBlock(B, FalseBranchExitPMap);
    return !Builder.isAnalysisDisabled();
}
} // namespace clang
