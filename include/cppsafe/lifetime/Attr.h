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

class LifetimeIOAttr : public InheritableAttr {
    Expr* expr;

public:
    enum Spelling {
        CXX11_gsl_lifetime_in = 0,
        CXX11_gsl_lifetime_out = 1,
        SpellingNotCalculated = 15

    };

    // Constructors
    LifetimeIOAttr(ASTContext& Ctx, const AttributeCommonInfo& CommonInfo, Expr* Expr);

    LifetimeIOAttr* clone(ASTContext& C) const;
    void printPretty(raw_ostream& OS, const PrintingPolicy& Policy) const;
    const char* getSpelling() const;
    Spelling getSemanticSpelling() const;
    bool isIn() const { return getAttributeSpellingListIndex() == 0; }
    bool isOut() const { return getAttributeSpellingListIndex() == 1; }
    Expr* getExpr() const { return expr; }

public:
    SmallVector<const Expr*, 8> InLocExprs;
    SmallVector<const Expr*, 8> OutLocExprs;
    SmallVector<ContractVariable, 8> InVars;
    SmallVector<ContractVariable, 8> OutVars;
};

}
