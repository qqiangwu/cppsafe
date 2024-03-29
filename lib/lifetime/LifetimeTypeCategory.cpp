//=- LifetimeTypeCategory.cpp - Diagnose lifetime violations -*- C++ -*-======//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//

#include "cppsafe/lifetime/LifetimeTypeCategory.h"

#include "cppsafe/lifetime/Lifetime.h"
#include "cppsafe/lifetime/contract/Annotation.h"

#include <clang/AST/Attr.h>
#include <clang/AST/Attrs.inc>
#include <clang/AST/DeclCXX.h>
#include <clang/AST/DeclTemplate.h>
#include <clang/AST/Type.h>
#include <clang/Basic/LLVM.h>
#include <clang/Basic/OperatorKinds.h>
#include <clang/Basic/Specifiers.h>
#include <clang/Sema/Sema.h>
#include <llvm/ADT/STLExtras.h>

#include <array>
#include <cassert>
#include <cstddef>
#include <functional>
#include <map>
#include <optional>
#include <set>

// NOLINTNEXTLINE(cppcoreguidelines-macro-usage): legacy
#define CLASSIFY_DEBUG 0

using std::size_t;

namespace clang::lifetime {

static QualType getPointeeType(const Type* T);

template <typename T>
static bool hasMethodLike(const CXXRecordDecl* R, T Predicate, const CXXMethodDecl** FoundMD = nullptr)
{
    // TODO cache IdentifierInfo to avoid string compare
    auto CallBack = [Predicate, FoundMD](const CXXRecordDecl* Base) {
        return std::none_of(Base->decls_begin(), Base->decls_end(), [Predicate, FoundMD](const Decl* D) {
            if (const auto* M = dyn_cast<CXXMethodDecl>(D)) {
                const bool Found = Predicate(M);
                if (Found && FoundMD) {
                    *FoundMD = M;
                }
                return Found;
            }
            if (const auto* Tmpl = dyn_cast<FunctionTemplateDecl>(D)) {
                if (auto* M = dyn_cast<CXXMethodDecl>(Tmpl->getTemplatedDecl())) {
                    const bool Found = Predicate(M);
                    if (Found && FoundMD) {
                        *FoundMD = M;
                    }
                    return Found;
                }
            }
            return false;
        });
    };
    return !R->forallBases(CallBack) || !CallBack(R);
}

static bool hasOperator(const CXXRecordDecl* R, OverloadedOperatorKind Op, int NumParams = -1, bool ConstOnly = false,
    const CXXMethodDecl** FoundMD = nullptr)
{
    return hasMethodLike(
        R,
        [Op, NumParams, ConstOnly](const CXXMethodDecl* MD) {
            if (NumParams != -1 && NumParams != (int)MD->param_size()) {
                return false;
            }
            if (ConstOnly && !MD->isConst()) {
                return false;
            }
            return MD->getOverloadedOperator() == Op;
        },
        FoundMD);
}

static bool hasMethodWithNameAndArgNum(
    const CXXRecordDecl* R, StringRef Name, int ArgNum = -1, const CXXMethodDecl** FoundMD = nullptr)
{
    return hasMethodLike(
        R,
        [Name, ArgNum](const CXXMethodDecl* M) {
            if (ArgNum >= 0 && (unsigned)ArgNum != M->getMinRequiredArguments()) {
                return false;
            }
            auto* I = M->getDeclName().getAsIdentifierInfo();
            if (!I) {
                return false;
            }
            return I->getName() == Name;
        },
        FoundMD);
}

static bool satisfiesContainerRequirements(const CXXRecordDecl* R)
{
    // TODO https://en.cppreference.com/w/cpp/named_req/Container
    return hasMethodWithNameAndArgNum(R, "begin", 0) && hasMethodWithNameAndArgNum(R, "end", 0)
        && !R->hasTrivialDestructor();
}

static bool satisfiesIteratorRequirements(const CXXRecordDecl* R)
{
    // TODO https://en.cppreference.com/w/cpp/named_req/Iterator
    // TODO operator* might be defined as a free function.
    return hasOperator(R, OO_Star, 0) && hasOperator(R, OO_PlusPlus, 0);
}

static bool satisfiesRangeConcept(const CXXRecordDecl* R)
{
    // TODO https://en.cppreference.com/w/cpp/experimental/ranges/range/Range
    return hasMethodWithNameAndArgNum(R, "begin", 0) && hasMethodWithNameAndArgNum(R, "end", 0)
        && R->hasTrivialDestructor();
}

static bool hasDerefOperations(const CXXRecordDecl* R)
{
    // TODO operator* might be defined as a free function.
    return hasOperator(R, OO_Arrow, 0) || hasOperator(R, OO_Star, 0);
}

/// Determines if D is std::vector<bool>::reference
static bool isVectorBoolReference(const CXXRecordDecl* D)
{
    assert(D);
    static const std::set<StringRef> StdVectorBoolReference { "__bit_const_reference" /* for libc++ */,
        "__bit_reference" /* for libc++ */, "_Bit_reference" /* for libstdc++ */, "_Vb_reference" /* for MSVC */ };
    if (!D->isInStdNamespace() || !D->getIdentifier()) {
        return false;
    }
    return StdVectorBoolReference.contains(D->getName());
}

/// Classifies some well-known std:: types or returns an empty optional.
/// Checks the type both before and after desugaring.
// TODO:
// Unfortunately, the types are stored in a desugared form for template
// instantiations. For this and some other reasons I think it would be better
// to look up the declarations (pointers) by names upfront and look up the
// declarations instead of matching strings populated lazily.
static std::optional<TypeCategory> classifyStd(const Type* T)
{
    auto* Decl = T->getAsCXXRecordDecl();
    if (!Decl || !Decl->isInStdNamespace() || !Decl->getIdentifier()) {
        return {};
    }

    if (isVectorBoolReference(Decl)) {
        return TypeCategory::Pointer;
    }

    return {};
}

/// Checks if all bases agree to the same TypeClassification,
/// and if they do, returns it.
static std::optional<TypeClassification> getBaseClassification(const CXXRecordDecl* R)
{
    QualType PointeeType;
    bool HasOwnerBase = false;
    bool HasPointerBase = false;
    bool PointeesDisagree = false;

    for (const CXXBaseSpecifier& B : R->bases()) {
        auto C = classifyTypeCategory(B.getType());
        if (C.TC == TypeCategory::Owner) {
            HasOwnerBase = true;
        } else if (C.TC == TypeCategory::Pointer) {
            HasPointerBase = true;
        } else {
            continue;
        }
        if (PointeeType.isNull()) {
            PointeeType = C.PointeeType;
        } else if (PointeeType != C.PointeeType) {
            PointeesDisagree = true;
        }
    }

    if (!HasOwnerBase && !HasPointerBase) {
        return {};
    }

    if (HasOwnerBase && HasPointerBase) {
        return TypeClassification(TypeCategory::Value);
    }

    if (PointeesDisagree) {
        return TypeClassification(TypeCategory::Value);
    }

    assert(HasOwnerBase ^ HasPointerBase);
    if (HasPointerBase) {
        return TypeClassification(TypeCategory::Pointer, PointeeType);
    }

    return TypeClassification(TypeCategory::Owner, PointeeType);
}

static QualType resolvePointerOrOwnerType(CXXRecordDecl* D, TypeSourceInfo* T, QualType TypeInfered)
{
    if (T) {
        const auto Ty = T->getType();
        const auto* TemplateTy = dyn_cast<TemplateTypeParmType>(Ty.getTypePtr());

        if (!TemplateTy) {
            return Ty;
        }

        return cast<ClassTemplateSpecializationDecl>(D)->getTemplateArgs().get(TemplateTy->getIndex()).getAsType();
    }

    if (!TypeInfered.isNull()) {
        return TypeInfered;
    }

    return D->getASTContext().VoidTy;
}

// NOLINTNEXTLINE(readability-function-cognitive-complexity): legacy code
static TypeClassification classifyTypeCategoryImpl(const Type* T)
{
    assert(T);
    auto* R = T->getAsCXXRecordDecl();

    if (!R) {
        if (T->isVoidPointerType()) {
            return TypeCategory::Value;
        }

        if (T->isArrayType()) {
            return { TypeCategory::Owner, getPointeeType(T) };
        }

        // raw pointers and references
        if (T->isPointerType() || T->isReferenceType()) {
            return { TypeCategory::Pointer, getPointeeType(T) };
        }

        return TypeCategory::Value;
    }

    if (!R->hasDefinition()) {
        if (auto* T = dyn_cast<ClassTemplateSpecializationDecl>(R)) {
            getSema()->InstantiateClassTemplateSpecialization(
                R->getSourceRange().getBegin(), T, TemplateSpecializationKind::TSK_ExplicitInstantiationDeclaration);
        }
    }

    if (!R->hasDefinition()) {
        return TypeCategory::Value;
    }

    if (R->isLambda()) {
        return { TypeCategory::Pointer, R->getASTContext().VoidTy };
    }

    if (R->isInStdNamespace() && R->getDeclName().isIdentifier()) {
        if (R->getName().ends_with("function")) {
            return { TypeCategory::Pointer, R->getASTContext().VoidTy };
        }
    }

    // In case we do not know the pointee type fall back to value.
    QualType Pointee = getPointeeType(T);

#if CLASSIFY_DEBUG
    llvm::errs() << "classifyTypeCategory " << QualType(T, 0).getAsString() << "\n";
    llvm::errs() << "  satisfiesContainerRequirements(R): " << satisfiesContainerRequirements(R) << "\n";
    llvm::errs() << "  hasDerefOperations(R): " << hasDerefOperations(R) << "\n";
    llvm::errs() << "  satisfiesRangeConcept(R): " << satisfiesRangeConcept(R) << "\n";
    llvm::errs() << "  hasTrivialDestructor(R): " << R->hasTrivialDestructor() << "\n";
    llvm::errs() << "  satisfiesIteratorRequirements(R): " << satisfiesIteratorRequirements(R) << "\n";
    llvm::errs() << "  R->isAggregate(): " << R->isAggregate() << "\n";
    llvm::errs() << "  R->isLambda(): " << R->isLambda() << "\n";
    llvm::errs() << "  DerefType: " << Pointee.getAsString() << "\n";
#endif

    if (const auto* A = R->getAttr<OwnerAttr>()) {
        Pointee = resolvePointerOrOwnerType(R, A->getDerefTypeLoc(), Pointee);
        return { TypeCategory::Owner, Pointee };
    }

    if (const auto* A = R->getAttr<PointerAttr>()) {
        Pointee = resolvePointerOrOwnerType(R, A->getDerefTypeLoc(), Pointee);
        return { TypeCategory::Pointer, Pointee };
    }

    // Do not attempt to infer implicit Pointer/Owner if we cannot deduce
    // the DerefType.
    if (!Pointee.isNull()) {
        // for std::unique_ptr<char[]>
        if (const auto* T = dyn_cast<ClassTemplateSpecializationDecl>(R)) {
            const auto* TD = T->getSpecializedTemplate()->getTemplatedDecl();
            if (TD->hasAttr<OwnerAttr>()) {
                return { TypeCategory::Owner, Pointee };
            }
            if (TD->hasAttr<PointerAttr>()) {
                return { TypeCategory::Pointer, Pointee };
            }
        }

        if (auto Cat = classifyStd(T)) {
            return { *Cat, Pointee };
        }

        // Every type that satisfies the standard Container requirements.
        if (satisfiesContainerRequirements(R)) {
            return { TypeCategory::Owner, Pointee };
        }

        // Every type that provides unary * or -> and has a user-provided
        // destructor. (Example: unique_ptr.)
        if (hasDerefOperations(R) && !R->hasTrivialDestructor()) {
            return { TypeCategory::Owner, Pointee };
        }

        //  Every type that satisfies the Ranges TS Range concept.
        if (satisfiesRangeConcept(R)) {
            return { TypeCategory::Pointer, Pointee };
        }

        // Every type that satisfies the standard Iterator requirements. (Example:
        // regex_iterator.), see
        // https://en.cppreference.com/w/cpp/named_req/Iterator
        if (satisfiesIteratorRequirements(R)) {
            return { TypeCategory::Pointer, Pointee };
        }

        // Every type that provides unary * or -> and does not have a user-provided
        // destructor. (Example: span.)
        if (hasDerefOperations(R) && R->hasTrivialDestructor()) {
            return { TypeCategory::Pointer, Pointee };
        }
    }

    // Every closure type of a lambda that captures by reference.
    if (R->isLambda()) {
        return TypeCategory::Value;
    }

    if (auto C = getBaseClassification(R)) {
        return *C;
    }

    // An Aggregate is a type that is not an Indirection
    // and is a class type with public data members
    // and no user-provided copy or move operations.
    if (R->isAggregate()) {
        return TypeCategory::Aggregate;
    }

    // A Value is a type that is neither an Indirection nor an Aggregate.
    return TypeCategory::Value;
}

TypeClassification classifyTypeCategory(const Type* T)
{
    static std::map<const Type*, TypeClassification> Cache;
    T = T->getUnqualifiedDesugaredType();

    auto I = Cache.find(T);
    if (I != Cache.end()) {
        return I->second;
    }

    auto TC = classifyTypeCategoryImpl(T);
    Cache.emplace(T, TC);
#if CLASSIFY_DEBUG
    llvm::errs() << "classifyTypeCategory(" << QualType(T, 0).getAsString() << ") = " << TC.str() << "\n";
#endif
    return TC;
}

bool isIteratorOrContainer(QualType QT)
{
    if (QT.isNull()) {
        return false;
    }

    static std::map<const Type*, bool> Cache;
    const auto* RawT = QT.getTypePtr();
    const auto* T = RawT->getUnqualifiedDesugaredType();
    auto It = Cache.find(T);
    if (It != Cache.end()) {
        return It->second;
    }

    const auto Ret = std::invoke([&] {
        if (QT.getAsString().ends_with("iterator")) {
            return true;
        }
        if (const auto* R = T->getAsCXXRecordDecl()) {
            if (satisfiesContainerRequirements(R)) {
                return true;
            }
        }

        return false;
    });

    Cache.emplace(T, Ret);
    return Ret;
}

// TODO: check gsl namespace?
// TODO: handle clang nullability annotations?
// NOLINTNEXTLINE(readability-function-cognitive-complexity): legacy
bool isNullableType(QualType QT)
{
    auto GetKnownNullability = [](StringRef Name) -> std::optional<bool> {
        if (Name == "nullable") {
            return true;
        }
        if (Name == "not_null") {
            return false;
        }
        return {};
    };
    QualType Inner = QT;
    if (const auto* TemplSpec = Inner->getAs<TemplateSpecializationType>()) {
        if (TemplSpec->isTypeAlias()) {
            if (const auto* TD = TemplSpec->getTemplateName().getAsTemplateDecl()) {
                if (TD->getIdentifier()) {
                    if (auto Nullability = GetKnownNullability(TD->getName())) {
                        return *Nullability;
                    }
                }
            }
        }
    }
    while (const auto* TypeDef = Inner->getAs<TypedefType>()) {
        const NamedDecl* Decl = TypeDef->getDecl();
        if (Decl->getName().ends_with_insensitive("iterator")) {
            return false;
        }
        if (isAnnotatedWith(Decl, "gsl::lifetime_nonnull")) {
            return false;
        }
        if (auto Nullability = GetKnownNullability(Decl->getName())) {
            return *Nullability;
        }
        Inner = TypeDef->desugar();
    }
    if (const auto* RD = Inner->getAsCXXRecordDecl()) {
        if (RD->isLambda()) {
            // allow capture pointers which can be nullptr
            return true;
        }
        if (!classifyTypeCategory(QT).isPointer()) {
            return false;
        }
        return hasMethodLike(RD, [](const CXXMethodDecl* D) {
            if (const auto* C = dyn_cast<CXXConversionDecl>(D)) {
                return C->getConversionType()->isBooleanType();
            }

            return false;
        });
    }
    return classifyTypeCategory(QT) == TypeCategory::Pointer && !QT->isReferenceType();
}

static QualType getPointeeType(const CXXRecordDecl* R)
{
    assert(R);

    for (auto* D : R->decls()) {
        if (const auto* TypeDef = dyn_cast<TypedefNameDecl>(D)) {
            if (TypeDef->getName() == "value_type") {
                return TypeDef->getUnderlyingType().getCanonicalType();
            }
        }
    }

    // TODO operator* might be defined as a free function.
    struct OpTy {
        OverloadedOperatorKind Kind;
        int ParamNum;
        bool ConstOnly;
    };
    constexpr std::array<OpTy, 3> Ops { {
        { OO_Star, 0, false },
        { OO_Arrow, 0, false },
        { OO_Subscript, -1, true },
    } };
    for (auto P : Ops) {
        const CXXMethodDecl* F = nullptr;
        if (hasOperator(R, P.Kind, P.ParamNum, P.ConstOnly, &F) && !F->isDependentContext()) {
            auto PointeeType = F->getReturnType();
            if (PointeeType->isReferenceType() || PointeeType->isAnyPointerType()) {
                PointeeType = PointeeType->getPointeeType();
            }
            if (P.ConstOnly) {
                return PointeeType.getCanonicalType().getUnqualifiedType();
            }
            return PointeeType.getCanonicalType();
        }
    }

    const CXXMethodDecl* FoundMD = nullptr;
    if (hasMethodWithNameAndArgNum(R, "begin", 0, &FoundMD)) {
        auto PointeeType = FoundMD->getReturnType();
        if (classifyTypeCategory(PointeeType) != TypeCategory::Pointer) {
#if CLASSIFY_DEBUG
            // TODO: diag?
            llvm::errs() << "begin() function does not return a Pointer!\n";
            FoundMD->dump();
#endif
            return {};
        }
        PointeeType = getPointeeType(PointeeType);
        return PointeeType.getCanonicalType();
    }

    return {};
}

static QualType getPointeeTypeImpl(const Type* T)
{
    if (T->isReferenceType() || T->isAnyPointerType()) {
        return T->getPointeeType();
    }

    if (T->isArrayType()) {
        // TODO: use AstContext.getAsArrayType() to correctly promote qualifiers
        const auto* AT = cast<ArrayType>(T);
        return AT->getElementType();
    }

    auto* R = T->getAsCXXRecordDecl();
    if (!R) {
        return {};
    }

    // std::vector<bool> contains std::vector<bool>::references
    if (isVectorBoolReference(R)) {
        return R->getASTContext().BoolTy;
    }

    if (!R->hasDefinition()) {
        return {};
    }

    auto PointeeType = getPointeeType(R);
    if (!PointeeType.isNull()) {
        return PointeeType;
    }

    if (auto* T = dyn_cast<ClassTemplateSpecializationDecl>(R)) {
        const auto& Args = T->getTemplateArgs();
        if (Args.size() > 0 && Args[0].getKind() == TemplateArgument::Type) {
            return Args[0].getAsType();
        }
    }
    return {};
}

/// WARNING: This overload does not consider base classes.
/// Use classifyTypeCategory(T).PointeeType to consider base classes.
static QualType getPointeeType(const Type* T)
{
    assert(T);
    T = T->getCanonicalTypeUnqualified().getTypePtr();
    static std::map<const Type*, QualType> M;

    auto I = M.find(T);
    if (I != M.end()) {
        return I->second;
    }

    // Insert Null before calling getPointeeTypeImpl to stop
    // a possible classifyTypeCategory -> getPointeeType infinite recursion
    M[T] = QualType {};

    auto P = getPointeeTypeImpl(T);
    if (!P.isNull()) {
        P = P.getCanonicalType();
    }
    M[T] = P;
#if CLASSIFY_DEBUG
    llvm::errs() << "DerefType(" << QualType(T, 0).getAsString() << ") = " << P.getAsString() << "\n";
#endif
    return P;
}

// NOLINTNEXTLINE(readability-function-cognitive-complexity)
bool isLifetimeConst(const FunctionDecl* FD, QualType Pointee, int ArgNum)
{
    // Until annotations are widespread, STL specific lifetimeconst
    // methods and params can be enumerated here.
    if (!FD) {
        return false;
    }

    // std::begin, std::end free functions.
    if (FD->isInStdNamespace() && FD->getDeclName().isIdentifier()
        && (FD->getName() == "begin" || FD->getName() == "end" || FD->getName() == "get" || FD->getName() == "forward"
            || FD->getName() == "move")) {
        return true;
    }

    if (ArgNum >= 0) {
        if (static_cast<size_t>(ArgNum) >= FD->param_size()) {
            return false;
        }

        const auto* Param = FD->parameters()[ArgNum];
        return Pointee.isConstQualified() || isLifetimeConst(Param);
    }

    assert(ArgNum == -1);
    if (isLifetimeConst(FD)) {
        return true;
    }

    if (const auto* MD = dyn_cast<CXXMethodDecl>(FD)) {
        if (MD->isConst()) {
            return true;
        }
        if (FD->isOverloadedOperator()) {
            return FD->getOverloadedOperator() == OO_Subscript || FD->getOverloadedOperator() == OO_Star
                || FD->getOverloadedOperator() == OO_Arrow;
        }
        const CXXRecordDecl* RD = MD->getParent();
        const StringRef ClassName = RD->getName();
        if (RD->isInStdNamespace()) {
            static constexpr auto MapConstFn = std::to_array<StringRef>({
                "insert",
                "emplace",
                "emplace_hint",
                "find",
                "lower_bound",
                "upper_bound",
            });
            if (ClassName.endswith("map") || ClassName.endswith("set")) {
                if (FD->getDeclName().isIdentifier() && (llvm::is_contained(MapConstFn, FD->getName()))) {
                    return true;
                }
            }
            if (ClassName == "list" || ClassName == "forward_list") {
                return FD->getDeclName().isIdentifier() && FD->getName() != "clear" && FD->getName() != "assign";
            }
        }

        static constexpr auto ContainerConstFn = std::to_array<StringRef>({
            "at",
            "data",
            "begin",
            "end",
            "rbegin",
            "rend",
            "front",
            "back",
        });
        return FD->getDeclName().isIdentifier() && llvm::is_contained(ContainerConstFn, FD->getName());
    }
    return false;
}
} // namespace clang::lifetime
