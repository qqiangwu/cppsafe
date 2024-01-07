#pragma once

#include "cppsafe/Options.h"

#include <clang/AST/Decl.h>
#include <clang/Sema/Sema.h>
#include <clang/Sema/SemaConsumer.h>

namespace cppsafe {

class AstConsumer : public clang::SemaConsumer {
public:
    explicit AstConsumer(const CppsafeOptions& Options)
        : Options(Options)
    {
    }

    void InitializeSema(clang::Sema& S) override { Sema = &S; }

    void ForgetSema() override { Sema = nullptr; }

    bool HandleTopLevelDecl(clang::DeclGroupRef D) override;

private:
    void run(const clang::FunctionDecl* Fn);

private:
    CppsafeOptions Options;
    clang::Sema* Sema = nullptr;
};

}
