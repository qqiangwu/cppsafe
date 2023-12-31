#pragma once

#include "cppsafe/lifetime/LifetimeAttrData.h"

#include <clang/AST/ASTContext.h>
#include <clang/AST/Attr.h>

namespace clang {

class LifetimeContractAttr {
public:
    using PointsToMap = std::map<ContractVariable, ContractPSet>;
    SmallVector<const Expr*, 8> PreExprs;
    SmallVector<const Expr*, 8> PostExprs;
    PointsToMap PrePSets;
    PointsToMap PostPSets;
};

}
