#pragma once

#include "cppsafe/lifetime/LifetimeAttrData.h"

#include <clang/AST/Decl.h>
#include <clang/AST/Type.h>
#include <clang/Basic/SourceLocation.h>

namespace clang::lifetime {

// Easier access the attribute's representation.
using AttrPointsToMap = std::map<ContractVariable, ContractPSet>;

SourceRange adjustParamContracts(StringRef Kind, const FunctionDecl* FD, const ParmVarDecl* PVD, AttrPointsToMap& Fill,
    const AttrPointsToMap& Lookup);

SourceRange adjustContracts(
    StringRef Kind, const FunctionDecl* FD, AttrPointsToMap& Fill, const AttrPointsToMap& Lookup);

SourceRange adjustCaptureContracts(const FunctionDecl* FD, AttrPointsToMap& Fill, const AttrPointsToMap& Lookup);

}
