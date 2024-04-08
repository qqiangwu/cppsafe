#include "cppsafe/lifetime/contract/CallVisitor.h"

#include "cppsafe/lifetime/Lifetime.h"
#include "cppsafe/lifetime/LifetimePset.h"
#include "cppsafe/lifetime/LifetimePsetBuilder.h"
#include "cppsafe/lifetime/LifetimeTypeCategory.h"
#include "cppsafe/lifetime/type/Aggregate.h"

#include <clang/AST/ASTContext.h>
#include <clang/AST/Attr.h>
#include <clang/AST/Attrs.inc>
#include <clang/AST/Decl.h>
#include <clang/AST/DeclCXX.h>
#include <clang/AST/Expr.h>
#include <clang/AST/ExprCXX.h>
#include <clang/AST/Type.h>
#include <clang/Basic/LLVM.h>
#include <clang/Basic/OperatorKinds.h>
#include <clang/Basic/SourceLocation.h>
#include <llvm/ADT/STLExtras.h>

#include <cassert>

namespace clang::lifetime {

namespace {

const FunctionDecl* getDecl(const Expr* CE)
{
    if (const auto* CallE = dyn_cast<CallExpr>(CE)) {
        return CallE->getDirectCallee();
    }

    const auto* Ctor = dyn_cast<CXXConstructExpr>(CE)->getConstructor();
    CPPSAFE_ASSERT(Ctor != nullptr);

    return Ctor;
}

template <typename PC, typename TC>
void forEachArgParamPair(const Expr* CE, const PC& ParamCallback, const TC& ThisCallback)
{
    const FunctionDecl* FD = getDecl(CE);
    assert(FD);

    ArrayRef Args = std::invoke([CE] {
        if (const auto* CallE = dyn_cast<CallExpr>(CE)) {
            return ArrayRef { CallE->getArgs(), CallE->getNumArgs() };
        }

        const auto* Ctor = cast<CXXConstructExpr>(CE);
        return ArrayRef { Ctor->getArgs(), Ctor->getNumArgs() };
    });

    const Expr* ObjectArg = nullptr;
    if (isa<CXXOperatorCallExpr>(CE) && FD->isCXXInstanceMember()) {
        ObjectArg = Args[0];
        Args = Args.slice(1);
    } else if (const auto* MCE = dyn_cast<CXXMemberCallExpr>(CE)) {
        ObjectArg = MCE->getImplicitObjectArgument();
    }

    unsigned Pos = 0;
    for (const Expr* Arg : Args) {
        // This can happen for c style var arg functions
        if (Pos >= FD->getNumParams()) {
            ParamCallback(nullptr, Arg, Pos);
        } else {
            const ParmVarDecl* PVD = FD->getParamDecl(Pos);
            ParamCallback(PVD, Arg, Pos);
        }
        ++Pos;
    }
    if (ObjectArg) {
        const CXXRecordDecl* RD = cast<CXXMethodDecl>(FD)->getParent();
        ThisCallback(Variable::thisPointer(RD), RD, ObjectArg);
    }
}

}

void CallVisitor::run(const CallExpr* CallE, ASTContext& ASTCtxt, const IsConvertibleTy& IsConvertible)
{
    // Default return value, will be overwritten if it makes sense.
    Builder.setPSet(CallE, {});

    if (isa<CXXPseudoDestructorExpr>(CallE->getCallee())) {
        return;
    }

    // TODO: function pointers are not handled. We need to get the contracts
    //       from the declaration of the function pointer somehow.
    const auto* Callee = CallE->getDirectCallee();
    if (!Callee) {
        return;
    }

    if (Callee->hasAttr<BuiltinAttr>() && Callee->getName() == "__builtin_addressof") {
        const auto* O = CallE->getArg(0);
        Builder.setPSet(CallE, Builder.getPSet(O));
        return;
    }

    /// Special case for assignment of Pointer into Pointer: copy pset
    if (handlePointerCopy(CallE)) {
        return;
    }
    if (handleAggregateCopy(CallE)) {
        enforcePreAndPostConditions(CallE, ASTCtxt, IsConvertible);
        return;
    }

    // p 2.4.5
    // When an Owner x is copied to or moved to or passed by lvalue reference to non-const to a function that has no
    // precondition that mentions x, set pset(x) = {x'}. When a Value x is copied to or moved to or passed by lvalue
    // reference to non-const to a function that has no precondition that mentions x, set pset(x) = {x}. These pset
    // resets are done before processing the function call (§2.5) including preconditions
    tryResetPSet(CallE);

    enforcePreAndPostConditions(CallE, ASTCtxt, IsConvertible);
}

void CallVisitor::enforcePreAndPostConditions(
    const Expr* CallE, ASTContext& ASTCtxt, const IsConvertibleTy& IsConvertible)
{
    const auto* Callee = getDecl(CallE);

    // Get preconditions.
    PSetsMap PreConditions;
    getLifetimeContracts(PreConditions, Callee, ASTCtxt, CurrentBlock, IsConvertible, Reporter, /*Pre=*/true);
    bindArguments(PreConditions, PreConditions, CallE);

    checkPreconditions(CallE, PreConditions);

    // if -Wlifetime-call-null is disabled, treat all input pointers as non-null,
    // to avoid any possibly-null warnings caused by parameters.
    if (!Reporter.getOptions().LifetimeCallNull) {
        for (auto& [_, P] : PreConditions) {
            P.removeNull();
        }
    }

    PSetsMap PostConditions;
    getLifetimeContracts(PostConditions, Callee, ASTCtxt, CurrentBlock, IsConvertible, Reporter, /*Pre=*/false);
    bindArguments(PostConditions, PreConditions, CallE, /*Checking=*/false);

    // PSets might become empty during the argument binding.
    // E.g.: when the pset(null) is bind to a non-null pset.
    // Also remove null outputs for non-null types.
    for (auto& Pair : PostConditions) {
        // TODO: Currently getType() fails when isReturnVal() is true because the
        // Variable does not store the type of the ReturnVal.
        const QualType OutputType = Pair.first.isReturnVal() ? Callee->getReturnType() : Pair.first.getType();
        if (!isNullableType(OutputType)) {
            Pair.second.removeNull();
        }
        if (Pair.second.isUnknown()) {
            Pair.second.addGlobal();
        }
    }

    // if -Wlifetime-call-null is disabled, treat all output pointers as non-null,
    // to avoid any possibly-null warnings caused by function calls
    if (!Reporter.getOptions().LifetimeCallNull) {
        for (auto& [V, P] : PostConditions) {
            P.removeNull();
        }
    }
    enforcePostconditions(CallE, Callee, PostConditions);
}

bool CallVisitor::handlePointerCopy(const CallExpr* CallE)
{
    if (const auto* OC = dyn_cast<CXXOperatorCallExpr>(CallE)) {
        if (OC->getOperator() == OO_Equal && OC->getNumArgs() == 2 && isPointer(OC->getArg(0))) {
            const auto* RArg = OC->getArg(1);
            const auto TC = classifyTypeCategory(RArg->getType());

            if (TC.isPointer() || isa<CXXNullPtrLiteralExpr>(RArg)) {
                const SourceRange Range = CallE->getSourceRange();
                PSet RHS = Builder.getPSet(Builder.getPSet(RArg));
                RHS = Builder.handlePointerAssign(OC->getArg(0)->getType(), RHS, Range);
                Builder.setPSet(Builder.getPSet(OC->getArg(0)), RHS, Range);
                Builder.setPSet(CallE, RHS);
                return true;
            }

            const PSet P = PSet::globalVar();
            Builder.setPSet(Builder.getPSet(OC->getArg(0)), P, CallE->getSourceRange());
            Builder.setPSet(CallE, P);
            return true;
        }
    }

    return false;
}

bool CallVisitor::handleAggregateCopy(const CallExpr* CallE)
{
    // TODO: what if user overrides assignment? WARN it
    const auto* OC = dyn_cast<CXXOperatorCallExpr>(CallE);
    if (!OC || OC->getOperator() != OO_Equal || OC->getNumArgs() != 2
        || !classifyTypeCategory(OC->getArg(0)->getType()).isAggregate()) {
        return false;
    }

    using lifetime::handleAggregateCopy;

    const auto* RArg = OC->getArg(1);
    handleAggregateCopy(CallE, RArg, Builder);

    const auto LHSPSet = Builder.getPSet(OC->getArg(0));
    // LHS maybe `*t`
    for (const auto& LHSVar : LHSPSet.vars()) {
        expandAggregate(LHSVar, CallE->getType()->getAsCXXRecordDecl(),
            [&](const Variable& LhsSubVar, const SubVarPath& Path, TypeClassification TC) {
                const auto RhsSubVar = Variable(CallE).chainFields(Path);

                if (!TC.isPointer()) {
                    if (Path.empty()) {
                        // aggregate object itself
                        Builder.setVarPSet(LhsSubVar, PSet::singleton(LhsSubVar));
                        return;
                    }

                    Builder.clearVarPSet(LhsSubVar);
                    return;
                }

                auto RHSPSet = Builder.getVarPSet(RhsSubVar);
                if (!RHSPSet) {
                    //
                    return;
                }

                RHSPSet->transformVars([&](Variable V) {
                    if (V.asExpr()) {
                        V = V.replaceExpr(LHSVar);
                    }

                    return V;
                });

                Builder.setVarPSet(LhsSubVar, *RHSPSet);
            });
    }
    return true;
}

void CallVisitor::tryResetPSet(const CallExpr* CallE)
{
    const auto* Obj = getObjectNeedReset(CallE);
    if (Obj == nullptr) {
        return;
    }

    // for `x->reset()`, Obj is `x`, with type pointer to CXXRecord
    const auto* T = Obj->getType()->getPointeeCXXRecordDecl();
    const auto TC = classifyTypeCategory(Obj->getType());
    if ((!T && TC.isPointer()) || (T && classifyTypeCategory(T->getTypeForDecl()).isPointer())) {
        const bool Nullable = isNullableType(T ? T->getTypeForDecl()->getCanonicalTypeUnqualified() : Obj->getType());

        Builder.setPSet(Builder.getPSet(Obj), PSet::globalVar(Nullable), CallE->getSourceRange());
        return;
    }

    PSet P = Builder.getPSet(Obj);
    if (TC.isOwner() || (T && classifyTypeCategory(T->getTypeForDecl()).isOwner())) {
        if (!P.vars().empty()) {
            P = PSet::singleton(*P.vars().begin(), 1);
        }
    }

    Builder.setPSet(Builder.getPSet(Obj), P, CallE->getSourceRange());
}

const Expr* CallVisitor::getObjectNeedReset(const CallExpr* CallE)
{
    if (const auto* OC = dyn_cast<CXXOperatorCallExpr>(CallE)) {
        if (OC->getOperator() == OO_Equal) {
            return OC->getArg(0);
        }
    }

    // TODO: check precondition
    if (const auto* MC = dyn_cast<CXXMemberCallExpr>(CallE)) {
        const auto* MD = MC->getMethodDecl();
        if (MD->isInstance()) {
            if (MD->hasAttr<ReinitializesAttr>()) {
                return MC->getImplicitObjectArgument();
            }
            if (MD->getDeclName().isIdentifier()) {
                if (MD->getName() == "reset" || MD->getName() == "clear") {
                    return MC->getImplicitObjectArgument();
                }
            }
        }
    }

    return nullptr;
}

// In the contracts every PSets are expressed in terms of the ParmVarDecls.
// We need to translate this to the PSets of the arguments so we can check
// substitutability.
void CallVisitor::bindArguments(PSetsMap& Fill, const PSetsMap& Lookup, const Expr* CE, bool Checking)
{
    // The sources of null are the actuals, not the formals.
    if (!Checking) {
        // int* p = foo(&n);
        // p's nullness is derived from @n
        // int* p = get();
        // p is the same as postcondition
        for (auto& PSet : llvm::make_second_range(Fill)) {
            if (!PSet.isNullableGlobal()) {
                PSet.removeNull();
            }
        }
    }

    auto BindTwoDerefLevels = [this, &Lookup, Checking](Variable V, const PSet& PS, PSetsMap::value_type& Pair) {
        Pair.second.bind(V, PS, Checking);
        if (!Lookup.contains(V)) {
            return;
        }
        V.deref();
        Pair.second.bind(V, Builder.derefPSet(PS), Checking);
    };

    auto ReturnIt = Fill.find(Variable::returnVal());
    forEachArgParamPair(
        CE,
        [&](const ParmVarDecl* PVD, const Expr* Arg, int) {
            if (!PVD) {
                // PVD is a c-style vararg argument.
                return;
            }
            const PSet ArgPS = Builder.getPSet(Arg, /*AllowNonExisting=*/true);
            if (ArgPS.isUnknown()) {
                return;
            }
            Variable V = PVD;
            V.deref();
            for (auto& VarToPSet : Fill) {
                BindTwoDerefLevels(V, ArgPS, VarToPSet);
            }
            // Do the binding for the return value.
            if (ReturnIt != Fill.end()) {
                BindTwoDerefLevels(V, ArgPS, *ReturnIt);
            }
        },
        [&](Variable V, const CXXRecordDecl*, const Expr* ObjExpr) {
            // Do the binding for this and *this
            V.deref();
            for (auto& VarToPSet : Fill) {
                BindTwoDerefLevels(V, Builder.getPSet(ObjExpr), VarToPSet);
            }
        });
}

// NOLINTNEXTLINE(readability-function-cognitive-complexity): todo
void CallVisitor::checkPreconditions(const Expr* CallE, PSetsMap& PreConditions)
{
    // Check preconditions. We might have them 2 levels deep.
    forEachArgParamPair(
        CallE,
        [&](const ParmVarDecl* PVD, const Expr* Arg, int) {
            const PSet ArgPS = Builder.getPSet(Arg, /*AllowNonExisting=*/true);
            if (ArgPS.isUnknown()) {
                return;
            }
            if (!PVD) {
                // PVD is a c-style vararg argument
                if (ArgPS.containsInvalid()) {
                    if (!ArgPS.shouldBeFilteredBasedOnNotes(Reporter) || ArgPS.isInvalid()) {
                        Reporter.warnNullDangling(
                            WarnType::Dangling, Arg->getSourceRange(), ValueSource::Param, "", !ArgPS.isInvalid());
                        ArgPS.explainWhyInvalid(Reporter);
                    }
                    Builder.setPSet(Arg, PSet()); // Suppress further warnings.
                }
                return;
            }

            // if x is mentioned in PVD, and is not invalid, verify not moved
            checkUseAfterMove(PreConditions, PVD, Arg);

            if (PreConditions.contains(PVD)) {
                if (!ArgPS.checkSubstitutableFor(PreConditions[PVD], Arg->getSourceRange(), Reporter)) {
                    Builder.setPSet(Arg, PSet()); // Suppress further warnings.
                }
            }

            Variable V = PVD;
            V.deref();
            if (PreConditions.contains(V)) {
                Builder.derefPSet(ArgPS).checkSubstitutableFor(PreConditions[V], Arg->getSourceRange(), Reporter);
            }
        },
        [&](Variable V, const RecordDecl* RD, const Expr* ObjExpr) {
            // V is this
            // ArgPs is pset(this)
            // we will always verify pset(*this) is valid
            const PSet ArgPS = Builder.getPSet(ObjExpr);
            if (PreConditions.contains(V)) {
                if (!checkUseAfterMove(RD->getTypeForDecl(), Builder.derefPSet(ArgPS), ObjExpr->getSourceRange())) {
                    Builder.setPSet(ObjExpr, PSet()); // Suppress further warnings.
                }

                if (!ArgPS.checkSubstitutableFor(PreConditions[V], ObjExpr->getSourceRange(), Reporter)) {
                    Builder.setPSet(ObjExpr, PSet()); // Suppress further warnings.
                }
            }

            V.deref();
            if (PreConditions.contains(V)) {
                Builder.derefPSet(ArgPS).checkSubstitutableFor(PreConditions[V], ObjExpr->getSourceRange(), Reporter);
            }
        });
}

void CallVisitor::enforcePostconditions(const Expr* CallE, const FunctionDecl* Callee, PSetsMap& PostConditions)
{
    invalidateNonConstUse(CallE);

    // Bind Pointer return value.
    auto TC = classifyTypeCategory(Callee->getReturnType());
    if (TC.isPointer()) {
        Builder.setPSet(CallE, PostConditions[Variable::returnVal()]);
    }

    // Bind output arguments.
    forEachArgParamPair(
        CallE,
        [&](const ParmVarDecl* PVD, const Expr* Arg, int) {
            if (!PVD) {
                // C-style vararg argument.
                if (!hasPSet(Arg)) {
                    return;
                }
                const PSet ArgPS = Builder.getPSet(Arg);
                if (ArgPS.vars().empty()) {
                    return;
                }
                Builder.setPSet(ArgPS, PSet::globalVar(false), Arg->getSourceRange());
                return;
            }
            Variable V = PVD;
            V.deref();
            if (PostConditions.contains(V)) {
                Builder.setPSet(Builder.getPSet(Arg), PostConditions[V], Arg->getSourceRange());
            }
        },
        [&](Variable V, const RecordDecl*, const Expr* ObjExpr) {
            V.deref();
            if (PostConditions.contains(V)) {
                Builder.setPSet(Builder.getPSet(ObjExpr), PostConditions[V], ObjExpr->getSourceRange());
            }
        });
}

// p2.5.5
// For every Owner o in Oout, at each call site reset pset(argument(o)) = {o'}
void CallVisitor::invalidateNonConstUse(const Expr* CallE)
{
    const auto* Callee = getDecl(CallE);

    // Invalidate owners taken by Pointer to non-const.
    forEachArgParamPair(
        CallE,
        [&](const ParmVarDecl* PVD, const Expr* Arg, int Pos) {
            if (!PVD) { // C-style vararg argument.
                return;
            }
            const QualType Pointee = getPointeeType(PVD->getType());
            if (Pointee.isNull()) {
                return;
            }

            const auto TC = classifyTypeCategory(Pointee);
            if (TC.isPointer() || isLifetimeConst(Callee, Pointee, Pos)) {
                return;
            }

            invalidateVarOnNoConstUse(Arg, TC);
        },
        [&](const Variable&, const RecordDecl* RD, const Expr* ObjExpr) {
            const auto* RT = RD->getTypeForDecl();
            const auto TC = classifyTypeCategory(RT);
            if (TC.isPointer() || isLifetimeConst(Callee, QualType(RT, 0), -1)) {
                return;
            }

            invalidateVarOnNoConstUse(ObjExpr, TC);
        });
}

void CallVisitor::invalidateVarOnNoConstUse(const Expr* Arg, const TypeClassification& TC)
{
    // TODO: callA(T&) -> callB(T&&), how to cope with this
    const bool IsMovedFrom = Reporter.getOptions().LifetimeMove && Arg->isXValue();

    const PSet ArgPS = Builder.getPSet(Arg);
    for (const Variable& V : ArgPS.vars()) {
        const auto Reason = IsMovedFrom ? InvalidationReason::moved(Arg->getSourceRange(), CurrentBlock)
                                        : InvalidationReason::modified(Arg->getSourceRange(), CurrentBlock);

        // p2.3.5 Move
        // When an Owner && parameter is invoked so that the && has a pset of {x'}, the && is bound to x and x’s
        // data will be moved from, so as a postcondition (after the function call) KILL(x'). When a non-Pointer x
        // is moved from, set pset(x) = {invalid} as a postcondition of the function call.
        if (TC.isOwner()) {
            Builder.invalidateOwner(V, Reason);
        }

        // TODO: expand aggregate to invalidate sub-owners

        if (IsMovedFrom) {
            Builder.invalidateVar(V, Reason);
        }
    }
}

void CallVisitor::checkUseAfterMove(const PSetsMap& PreConditions, const ParmVarDecl* PVD, const Expr* Arg)
{
    // PVD: T*/T&/T&&/T*&/T*&&
    auto Ty = PVD->getType();
    Variable V = PVD;
    PSet P = Builder.getPSet(Arg);

    auto It = PreConditions.find(V);
    while (
        It != PreConditions.end() && !It->second.containsInvalid() && (Ty->isPointerType() || Ty->isReferenceType())) {
        if (!checkUseAfterMove(Ty->getPointeeType().getTypePtr(), Builder.derefPSet(P), Arg->getSourceRange())) {
            Builder.setPSet(Arg, PSet()); // Suppress further warnings.
            return;
        }

        V.deref();
        P = Builder.derefPSet(P);
        Ty = Ty->getPointeeType();
    }
}

bool CallVisitor::checkUseAfterMove(const Type* Ty, const PSet& P, SourceRange Range)
{
    if (!Reporter.getOptions().LifetimeMove) {
        return true;
    }

    const auto TC = classifyTypeCategory(Ty);
    if (TC.isPointer()) {
        return true;
    }

    if (!P.containsInvalid()) {
        return true;
    }

    Reporter.warn(WarnType::UseAfterMove, Range, false);
    P.explainWhyInvalid(Reporter);
    return false;
}

}
