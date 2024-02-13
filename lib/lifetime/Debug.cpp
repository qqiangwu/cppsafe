#include "cppsafe/lifetime/Debug.h"
#include "cppsafe/lifetime/Lifetime.h"
#include "cppsafe/lifetime/LifetimePset.h"
#include "cppsafe/lifetime/LifetimePsetBuilder.h"
#include "cppsafe/lifetime/LifetimeTypeCategory.h"
#include "cppsafe/lifetime/contract/CallVisitor.h"

#include <clang/AST/Attr.h>
#include <clang/AST/Decl.h>
#include <clang/AST/DeclBase.h>
#include <clang/AST/DeclCXX.h>
#include <clang/AST/DeclTemplate.h>
#include <clang/AST/Expr.h>
#include <clang/AST/ExprCXX.h>
#include <clang/AST/Stmt.h>
#include <clang/AST/Type.h>
#include <clang/Analysis/CFG.h>
#include <clang/Basic/LLVM.h>
#include <clang/Basic/SourceLocation.h>
#include <clang/Lex/Lexer.h>

#include <cassert>
#include <functional>
#include <string>

namespace clang::lifetime {

static bool debugPSetImpl(
    const bool Deref, const PSBuilder& Builder, const CallExpr* CallE, ASTContext& Ctx, LifetimeReporterBase& Reporter)
{
    assert(CallE->getNumArgs() == 1 && "__lifetime_pset takes one argument");

    const auto* Arg = CallE->getArg(0);
    PSet Set = Builder.getPSet(Arg);

    auto IsLocalRefVar = [](const Expr* E) {
        const auto* D = dyn_cast<DeclRefExpr>(E);
        if (!D) {
            return false;
        }

        const auto* V = D->getDecl()->getPotentiallyDecomposedVarDecl();
        if (!V || !V->getType()->isReferenceType()) {
            return false;
        }

        return V->isLocalVarDecl() && !isa<DecompositionDecl>(V);
    };
    if (Deref && !IsLocalRefVar(Arg)) {
        if (!hasPSet(Arg)) {
            return true; // Argument must be a Pointer or Owner
        }
        Set = Builder.getPSet(Set);
    }
    const StringRef SourceText = Lexer::getSourceText(
        CharSourceRange::getTokenRange(Arg->getSourceRange()), Ctx.getSourceManager(), Ctx.getLangOpts());
    Reporter.debugPset(CallE->getSourceRange(), SourceText, Set.str());
    return true;
}

bool debugPSet(const PSBuilder& Builder, const CallExpr* CallE, ASTContext& Ctx, LifetimeReporterBase& Reporter)
{
    return debugPSetImpl(true, Builder, CallE, Ctx, Reporter);
}

bool debugPSetRef(const PSBuilder& Builder, const CallExpr* CallE, ASTContext& Ctx, LifetimeReporterBase& Reporter)
{
    return debugPSetImpl(false, Builder, CallE, Ctx, Reporter);
}

bool debugTypeCategory(const CallExpr* CallE, QualType QType, LifetimeReporterBase& Reporter)
{
    const auto Range = CallE->getSourceRange();
    auto Class = classifyTypeCategory(QType);
    if (Class.isIndirection()) {
        Reporter.debugTypeCategory(Range, Class.TC, Class.PointeeType.getAsString());
    } else {
        Reporter.debugTypeCategory(Range, Class.TC);
    }
    return true;
}

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

bool debugContracts(const CallExpr* CallE, ASTContext& ASTCtxt, const CFGBlock* CurrentBlock,
    const IsConvertibleTy& IsConvertible, LifetimeReporterBase& Reporter)
{
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

    const auto Range = CallE->getSourceRange();
    auto PrintContract = [&Reporter, &Range](const auto& E, const std::string& Contract) {
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

}
