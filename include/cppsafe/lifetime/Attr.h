#pragma once

#include "cppsafe/lifetime/LifetimeAttrData.h"

#include <clang/AST/ASTContext.h>
#include <clang/AST/Attr.h>

namespace clang::lifetime {

class LifetimeContractAttr {
public:
    using PointsToMap = std::map<ContractVariable, ContractPSet>;

    PointsToMap PrePSets;
    PointsToMap PostPSets;

    bool Filled = false;
};

}
