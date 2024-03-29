#pragma once

#include "cppsafe/lifetime/Lifetime.h"
#include "cppsafe/lifetime/LifetimePset.h"
#include "cppsafe/lifetime/LifetimeTypeCategory.h"
#include "cppsafe/util/type.h"

#include <clang/AST/Decl.h>
#include <clang/AST/Expr.h>
#include <clang/Analysis/CFG.h>
#include <gsl/pointers>

namespace clang::lifetime {

class PSBuilder;

class CallVisitor {
public:
    CallVisitor(PSBuilder& B, LifetimeReporterBase& Reporter, gsl::not_null<const CFGBlock*> Block)
        : Builder(B)
        , Reporter(Reporter)
        , CurrentBlock(Block)
    {
    }

    ~CallVisitor() = default;

    DISALLOW_COPY_AND_MOVE(CallVisitor);

    void run(const CallExpr* CallE, ASTContext& ASTCtxt, const IsConvertibleTy& IsConvertible);

    // for constructors
    void enforcePreAndPostConditions(const Expr* CallE, ASTContext& ASTCtxt, const IsConvertibleTy& IsConvertible);

private:
    bool handlePointerCopy(const CallExpr* CallE);

    void tryResetPSet(const CallExpr* CallE);
    const Expr* getObjectNeedReset(const CallExpr* CallE);

    void bindArguments(PSetsMap& Fill, const PSetsMap& Lookup, const Expr* CE, bool Checking = true);

    void checkPreconditions(const Expr* CallE, PSetsMap& PreConditions);
    void enforcePostconditions(const Expr* CallE, const FunctionDecl* Callee, PSetsMap& PostConditions);

    void invalidateNonConstUse(const Expr* CallE);
    void invalidateVarOnNoConstUse(const Expr* Arg, const TypeClassification& TC);
    void checkUseAfterMove(const PSetsMap& PreConditions, const ParmVarDecl* PVD, const Expr* Arg);
    bool checkUseAfterMove(const Type* Ty, const PSet& P, SourceRange Range);

private:
    PSBuilder& Builder;
    LifetimeReporterBase& Reporter;
    gsl::not_null<const CFGBlock*> CurrentBlock;
};

}
