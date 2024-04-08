#pragma once

#include "cppsafe/lifetime/Lifetime.h"
#include "cppsafe/lifetime/LifetimePsetBuilder.h"
#include "cppsafe/lifetime/LifetimeTypeCategory.h"

#include <clang/AST/Decl.h>
#include <clang/AST/DeclCXX.h>
#include <clang/AST/Expr.h>
#include <clang/Analysis/CFG.h>
#include <clang/Basic/SourceLocation.h>
#include <llvm/ADT/STLFunctionalExtras.h>

namespace clang::lifetime {

void expandAggregate(const Variable& Base, const CXXRecordDecl* RD,
    llvm::function_ref<void(const Variable&, const FieldDecl*, TypeClassification)> Fn);

void handleAggregateDefaultInit(
    const Variable& Base, const CXXRecordDecl* RD, const CFGBlock* B, SourceRange Range, PSBuilder& Builder);

void handleAggregateInit(const Variable& Base, const CXXRecordDecl* RD, const InitListExpr* InitE, const CFGBlock* B,
    SourceRange Range, PSBuilder& Builder);

void handleAggregateCopy(const Expr* LHS, const Expr* RHS, PSBuilder& Builder);

}
