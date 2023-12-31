#pragma once

#include <clang/AST/Attr.h>
#include <clang/AST/DeclBase.h>
#include <llvm/ADT/StringRef.h>

namespace clang::lifetime {

inline bool isAnnotatedWith(const Decl* D, StringRef Category)
{
    if (const auto* A = D->getAttr<AnnotateAttr>()) {
        return A->getAnnotation() == Category;
    }

    return false;
}

inline bool isLifetimeConst(const Decl* D) { return isAnnotatedWith(D, "gsl::lifetime_const"); }

inline bool isLifetimeIn(const Decl* D) { return isAnnotatedWith(D, "gsl::lifetime_in"); }

}