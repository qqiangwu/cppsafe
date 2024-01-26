#pragma once

#include <clang/AST/Attr.h>
#include <clang/AST/Attrs.inc>
#include <clang/AST/DeclBase.h>
#include <llvm/ADT/STLExtras.h>
#include <llvm/ADT/StringRef.h>

namespace clang::lifetime {

inline constexpr StringRef LifetimePre = "gsl::lifetime_pre";
inline constexpr StringRef LifetimePost = "gsl::lifetime_post";

inline bool isAnnotatedWith(const Decl* D, StringRef Category)
{
    return llvm::any_of(D->specific_attrs<AnnotateAttr>(),
        [Category](const AnnotateAttr* A) { return A->getAnnotation() == Category; });
}

inline auto getAnnotatedWith(const Decl* D, StringRef Category)
{
    return llvm::make_filter_range(D->specific_attrs<AnnotateAttr>(),
        [Category](const AnnotateAttr* A) { return A->getAnnotation() == Category; });
}

inline bool isLifetimeConst(const Decl* D) { return isAnnotatedWith(D, "gsl::lifetime_const"); }

inline bool isLifetimeIn(const Decl* D) { return isAnnotatedWith(D, "gsl::lifetime_in"); }

inline bool isLifetimeInout(const Decl* D) { return isAnnotatedWith(D, "gsl::lifetime_inout"); }

inline bool isWarningSuppressed(const Decl* D)
{
    return llvm::any_of(D->specific_attrs<SuppressAttr>(),
        [](const SuppressAttr* A) { return llvm::is_contained(A->diagnosticIdentifiers(), "lifetime"); });
}
}
