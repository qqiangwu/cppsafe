//=- LifetimePsetBuilder.h - Diagnose lifetime violations -*- C++ -*-=========//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//

#ifndef LLVM_CLANG_ANALYSIS_ANALYSES_LIFETIMEPSETBUILDER_H
#define LLVM_CLANG_ANALYSIS_ANALYSES_LIFETIMEPSETBUILDER_H

#include "Lifetime.h"
#include "LifetimePset.h"
#include "cppsafe/util/type.h"

#include <clang/Basic/SourceLocation.h>

namespace clang {
class CFGBlock;
class ASTContext;

namespace lifetime {
class LifetimeReporterBase;

class PSBuilder {
public:
    PSBuilder() = default;

    virtual ~PSBuilder() = default;

    DISALLOW_COPY_AND_MOVE(PSBuilder);

    virtual void setPSet(const Expr* E, const PSet& PS) = 0;
    virtual void setPSet(const PSet& LHS, PSet RHS, SourceRange Range) = 0;

    virtual PSet getPSet(const Expr* E, bool AllowNonExisting = false) const = 0;
    virtual PSet getPSet(const Variable& V) const = 0;
    virtual PSet getPSet(const PSet& P) const = 0;

    virtual void debugPmap(SourceRange Range) const = 0;

    virtual PSet derefPSet(const PSet& P) const = 0;

    virtual PSet handlePointerAssign(QualType LHS, PSet RHS, SourceRange Range, bool AddReason = true) const = 0;

    virtual void invalidateOwner(const Variable& V, const InvalidationReason& Reason) = 0;
    virtual void invalidateVar(const Variable& V, const InvalidationReason& Reason) = 0;
};

/// Updates psets with all effects that appear in the block.
/// \param Reporter if non-null, emits diagnostics
/// \returns false when an unsupported AST node disabled the analysis
bool VisitBlock(const FunctionDecl* FD, PSetsMap& PMap, std::optional<PSetsMap>& FalseBranchExitPMap,
    std::map<const Expr*, PSet>& PSetsOfExpr, std::map<const Expr*, PSet>& RefersTo, const CFGBlock& B,
    LifetimeReporterBase& Reporter, ASTContext& ASTCtxt, IsConvertibleTy IsConvertible);

/// Get the initial PSets for function parameters.
void getLifetimeContracts(PSetsMap& PMap, const FunctionDecl* FD, const ASTContext& ASTCtxt, const CFGBlock* Block,
    IsConvertibleTy isConvertible, LifetimeReporterBase& Reporter, bool Pre = true, bool IgnoreNull = false,
    bool IgnoreFields = false);
} // namespace lifetime
} // namespace clang

#endif // LLVM_CLANG_ANALYSIS_ANALYSES_LIFETIMEPSETBUILDER_H
