#pragma once

#include <clang/AST/Attr.h>
#include <clang/AST/Attrs.inc>
#include <clang/AST/DeclBase.h>
#include <llvm/ADT/STLExtras.h>
#include <llvm/ADT/StringRef.h>

namespace clang::lifetime {

inline bool isAnnotatedWith(const Decl* D, StringRef Category)
{
    return llvm::any_of(D->specific_attrs<AnnotateAttr>(),
        [Category](const AnnotateAttr* A) { return A->getAnnotation() == Category; });
}

inline bool isLifetimeConst(const Decl* D) { return isAnnotatedWith(D, "gsl::lifetime_const"); }

inline bool isLifetimeIn(const Decl* D) { return isAnnotatedWith(D, "gsl::lifetime_in"); }

}
