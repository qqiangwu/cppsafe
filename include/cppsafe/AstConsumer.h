#pragma once

#include "cppsafe/lifetime/Lifetime.h"
#include "cppsafe/Options.h"

#include <clang/AST/Decl.h>
#include <clang/Basic/Specifiers.h>
#include <clang/Sema/Sema.h>
#include <clang/Sema/SemaConsumer.h>
#include <llvm/Support/Casting.h>

namespace cppsafe {

class AstConsumer : public clang::SemaConsumer {
public:
    explicit AstConsumer(const CppsafeOptions& Options)
        : Options(Options)
    {
    }

    void InitializeSema(clang::Sema& S) override { Sema = &S; }

    void ForgetSema() override { Sema = nullptr; }

    bool HandleTopLevelDecl(clang::DeclGroupRef D) override
    {
        for (const auto* Decl : D) {
            if (const auto* Fn = llvm::dyn_cast<clang::FunctionDecl>(Decl)) {
                run(Fn);
            }
        }

        return true;
    }

private:
    void run(const clang::FunctionDecl* Fn);

private:
    CppsafeOptions Options;
    clang::Sema* Sema = nullptr;
};

}
