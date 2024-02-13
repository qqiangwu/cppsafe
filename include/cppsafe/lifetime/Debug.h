#pragma once

#include "cppsafe/lifetime/Lifetime.h"
#include "cppsafe/lifetime/LifetimePsetBuilder.h"

#include <clang/AST/ASTContext.h>
#include <clang/AST/Expr.h>
#include <clang/AST/Type.h>
#include <clang/Analysis/CFG.h>

namespace clang::lifetime {

bool debugPSet(const PSBuilder& Builder, const CallExpr* CallE, ASTContext& Ctx, LifetimeReporterBase& Reporter);

bool debugPSetRef(const PSBuilder& Builder, const CallExpr* CallE, ASTContext& Ctx, LifetimeReporterBase& Reporter);

bool debugTypeCategory(const CallExpr* CallE, QualType QType, LifetimeReporterBase& Reporter);

bool debugContracts(const CallExpr* CallE, ASTContext& ASTCtxt, const CFGBlock* CurrentBlock,
    const IsConvertibleTy& IsConvertible, LifetimeReporterBase& Reporter);

}
