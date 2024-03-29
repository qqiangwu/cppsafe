//=- LifetimeTypeCategory.h - Diagnose lifetime violations -*- C++ -*-========//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//

#ifndef LLVM_CLANG_ANALYSIS_ANALYSES_LIFETIMETYPECATEGORY_H
#define LLVM_CLANG_ANALYSIS_ANALYSES_LIFETIMETYPECATEGORY_H

#include "cppsafe/lifetime/Lifetime.h"

#include "clang/AST/Type.h"

namespace clang::lifetime {

struct TypeClassification {
    TypeCategory TC;
    /// Called DerefType in the paper. Valid when TC is Owner or Pointer.
    QualType PointeeType;

    TypeClassification(TypeCategory TC)
        : TC(TC)
    {
        assert(TC == TypeCategory::Aggregate || TC == TypeCategory::Value);
    }

    TypeClassification(TypeCategory TC, QualType PointeeType)
        : TC(TC)
        , PointeeType(PointeeType)
    {
        assert(!PointeeType.isNull());
    }

    bool isPointer() const { return TC == TypeCategory::Pointer; }

    bool isOwner() const { return TC == TypeCategory::Owner; }

    bool isIndirection() const { return TC == TypeCategory::Pointer || TC == TypeCategory::Owner; }

    bool isAggregate() const { return TC == TypeCategory::Aggregate; }

    bool isValue() const { return TC == TypeCategory::Value; }

    std::string str() const
    {
        switch (TC) {
        case TypeCategory::Owner:
            return "Owner with DerefType " + PointeeType.getAsString();
        case TypeCategory::Pointer:
            return "Pointer with DerefType " + PointeeType.getAsString();
        case TypeCategory::Aggregate:
            return "Aggregate";
        case TypeCategory::Value:
            return "Value";
        }
    }

    bool operator==(TypeCategory O) const { return O == TC; }
    bool operator!=(TypeCategory O) const { return O != TC; }

    operator TypeCategory() const { return TC; }
};

/// Returns the type category of the given type
/// If T is a template specialization, it must be instantiated.
/// \post If the returned TypeCategory is Owner or Pointer, then
///       getPointeeType() will return non-null for the same QT.
TypeClassification classifyTypeCategory(const Type* T);

inline TypeClassification classifyTypeCategory(QualType QT) { return classifyTypeCategory(QT.getTypePtr()); }

bool isIteratorOrContainer(QualType QT);

bool isNullableType(QualType QT);

// For primitive types like pointers, references we return the pointee.
// For user defined types the pointee type is determined by the return
// type of operator*, operator-> or operator[]. Since these methods
// might return references, and operator-> returns a pointer, we strip
// off that one extra level of pointer/references.
inline QualType getPointeeType(QualType QT) { return classifyTypeCategory(QT).PointeeType; }

bool isLifetimeConst(const FunctionDecl* FD, QualType Pointee, int ArgNum);

inline bool isOwner(const Expr* E) { return classifyTypeCategory(E->getType()).isOwner(); }

inline bool isPointer(const Expr* E) { return classifyTypeCategory(E->getType()).isPointer(); }

inline bool isIndirection(const Expr* E) { return classifyTypeCategory(E->getType()).isIndirection(); }

inline bool hasPSet(const Expr* E)
{
    auto TC = classifyTypeCategory(E->getType());
    return E->isLValue() || TC.isIndirection();
}

} // namespace clang::lifetime

#endif // LLVM_CLANG_ANALYSIS_ANALYSES_LIFETIMETYPECATEGORY_H
