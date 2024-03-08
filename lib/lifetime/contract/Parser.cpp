#include "cppsafe/lifetime/contract/Parser.h"

#include "cppsafe/lifetime/LifetimeAttrData.h"
#include "cppsafe/lifetime/contract/Annotation.h"

#include <clang/AST/Attr.h>
#include <clang/AST/Decl.h>
#include <clang/AST/DeclCXX.h>
#include <clang/AST/Expr.h>
#include <clang/AST/ExprCXX.h>
#include <clang/Basic/LLVM.h>
#include <clang/Basic/SourceLocation.h>
#include <llvm/ADT/STLExtras.h>
#include <llvm/ADT/TypeSwitch.h>
#include <llvm/ADT/iterator_range.h>

#include <optional>

namespace clang::lifetime {

static const Expr* ignoreReturnValues(const Expr* E)
{
    const Expr* Original = nullptr;
    do { // NOLINT(cppcoreguidelines-avoid-do-while): legacy code
        Original = E;
        E = E->IgnoreImplicit();
        if (const auto* CE = dyn_cast<CXXConstructExpr>(E)) {
            E = CE->getArg(0);
        }
        if (const auto* MCE = dyn_cast<CXXMemberCallExpr>(E)) {
            const auto* CD = dyn_cast_or_null<CXXConversionDecl>(MCE->getDirectCallee());
            if (CD) {
                E = MCE->getImplicitObjectArgument();
            }
        }
    } while (E != Original);
    return E;
}

static const ParmVarDecl* toCanonicalParmVar(const ParmVarDecl* PVD)
{
    const auto* FD = dyn_cast<FunctionDecl>(PVD->getDeclContext());
    return FD->getCanonicalDecl()->getParamDecl(PVD->getFunctionScopeIndex());
}

static std::optional<ContractPSet> resolvePrefinedVar(StringRef Name)
{
    ContractPSet Result;

    if (Name == "null") {
        Result.ContainsNull = true;
        return Result;
    }
    if (Name == "global") {
        Result.ContainsGlobal = true;
        return Result;
    }
    if (Name == "invalid") {
        Result.ContainsInvalid = true;
        return Result;
    }

    return {};
}

// This function can either collect the PSets of the symbols based on a lookup
// table or just the symbols into a pset if the lookup table is nullptr.
static std::optional<ContractPSet> lookupVar(const FunctionDecl* FD, StringRef E, const AttrPointsToMap* Lookup)
{
    if (E.starts_with(":")) {
        return resolvePrefinedVar(E.drop_front());
    }
    if (E == "return") {
        ContractPSet Result;
        Result.Vars.insert(ContractVariable::returnVal());
        return Result;
    }

    int Deref = 0;
    while (!E.empty() && E.front() == '*') {
        ++Deref;
        E = E.drop_front();
    }

    if (E == "this") {
        const auto* MD = dyn_cast<CXXMethodDecl>(FD);
        if (!MD || !MD->isInstance()) {
            return {};
        }

        ContractVariable V(MD->getParent());
        V.deref(Deref);

        if (Lookup) {
            const auto It2 = Lookup->find(V);
            if (It2 == Lookup->end()) {
                return {};
            }

            return It2->second;
        }

        return V;
    }

    FD = FD->getCanonicalDecl();
    const auto* It = llvm::find_if(FD->parameters(), [E](const ParmVarDecl* PVD) { return PVD->getName() == E; });
    if (It == FD->param_end()) {
        return {};
    }

    // When we have a post condition like:
    //     pset(Return) == pset(a)
    // We need to look up the Pset of 'a' in preconditions but we need to
    // record the postcondition in the postconditions.
    ContractVariable V(*It);
    V.deref(Deref);

    if (Lookup) {
        const auto It2 = Lookup->find(V);
        if (It2 == Lookup->end()) {
            return {};
        }

        return It2->second;
    }

    return ContractPSet(V);
}

static std::optional<ContractPSet> resolveContractVar(
    const FunctionDecl* FD, const Expr* E, const AttrPointsToMap* Lookup)
{
    return llvm::TypeSwitch<const Expr*, std::optional<ContractPSet>>(ignoreReturnValues(E))
        .Case<StringLiteral>([=](const StringLiteral* S) -> std::optional<ContractPSet> {
            return lookupVar(FD, S->getString(), Lookup);
        })
        .Case<DeclRefExpr>([=](const DeclRefExpr* DRE) -> std::optional<ContractPSet> {
            const auto* VD = dyn_cast<VarDecl>(DRE->getDecl());
            if (!VD) {
                return {};
            }

            ContractPSet Result;
            const StringRef Name = VD->getName();
            if (Name == "Null") {
                Result.ContainsNull = true;
                return Result;
            }
            if (Name == "Global") {
                Result.ContainsGlobal = true;
                return Result;
            }
            if (Name == "Invalid") {
                Result.ContainsInvalid = true;
                return Result;
            }
            if (Name == "Return") {
                Result.Vars.insert(ContractVariable::returnVal());
                return Result;
            }

            return {};
        })
        .Default([](auto&&) { return std::nullopt; });
}

SourceRange adjustContracts(
    StringRef Kind, const FunctionDecl* FD, AttrPointsToMap& Fill, const AttrPointsToMap& Lookup)
{
    for (const auto* Attr : getAnnotatedWith(FD, Kind)) {
        if (Attr->args_size() < 2) {
            return Attr->getRange();
        }

        std::optional<ContractPSet> V = resolveContractVar(FD, *Attr->args_begin(), nullptr);
        if (!V || !V->isVar()) {
            return (*Attr->args_begin())->getSourceRange();
        }

        ContractPSet PS;
        for (const auto* E : llvm::make_range(Attr->args_begin() + 1, Attr->args_end())) {
            std::optional<ContractPSet> DependsOn = resolveContractVar(FD, E, &Lookup);
            if (!DependsOn) {
                return E->getSourceRange();
            }

            PS.merge(*DependsOn);
        }

        Fill[*V->Vars.begin()] = PS;
    }

    return {};
}

SourceRange adjustParamContracts(StringRef Kind, const FunctionDecl* FD, const ParmVarDecl* PVD, AttrPointsToMap& Fill,
    const AttrPointsToMap& Lookup)
{
    for (const auto* Attr : getAnnotatedWith(PVD, Kind)) {
        if (Attr->args_size() < 1) {
            return Attr->getRange();
        }

        ContractPSet PS;
        for (const auto* E : Attr->args()) {
            std::optional<ContractPSet> DependsOn = resolveContractVar(FD, E, &Lookup);
            if (!DependsOn) {
                return E->getSourceRange();
            }

            PS.merge(*DependsOn);
        }

        Fill[toCanonicalParmVar(PVD)] = PS;
    }

    return {};
}

}
