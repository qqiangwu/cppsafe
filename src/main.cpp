#include "cppsafe/AstConsumer.h"
#include "cppsafe/Options.h"

#include <clang/AST/ASTConsumer.h>
#include <clang/Frontend/FrontendAction.h>
#include <clang/Tooling/CommonOptionsParser.h>
#include <clang/Tooling/Tooling.h>
#include <llvm/ADT/StringRef.h>
#include <llvm/Support/CommandLine.h>
#include <llvm/Support/Error.h>
#include <llvm/Support/InitLLVM.h>
#include <llvm/Support/WithColor.h>

#include <memory>

using namespace clang::tooling;
using namespace llvm;
using namespace cppsafe;

static cl::desc desc(StringRef Description) { return { Description.ltrim() }; }

// NOLINTNEXTLINE(cppcoreguidelines-avoid-non-const-global-variables)
static cl::OptionCategory CppSafeCategory("cppsafe options");

static const cl::opt<bool> WarnNoLifetimeNull("Wno-lifetime-null",
    desc("Disable flow-sensitive warnings about nullness of Pointers"), cl::init(false), cl::cat(CppSafeCategory));

static const cl::opt<bool> WarnLifetimeGlobal("Wlifetime-global",
    desc("Flow-sensitive analysis that requires global variables of Pointer type to always point to variables with "
         "static lifetime"),
    cl::init(false), cl::cat(CppSafeCategory));

static const cl::opt<bool> WarnLifetimeDisabled("Wlifetime-disabled",
    desc("Get warnings when the flow-sensitive analysis is disabled on a function due to forbidden constructs"),
    cl::init(false), cl::cat(CppSafeCategory));

static const cl::opt<bool> WarnLifetimeOutput("Wlifetime-output",
    desc("Enforce output parameter validity check in all paths"), cl::init(false), cl::cat(CppSafeCategory));

class LifetimeFrontendAction : public clang::ASTFrontendAction {
public:
    std::unique_ptr<clang::ASTConsumer> CreateASTConsumer(clang::CompilerInstance&, llvm::StringRef) override
    {
        const CppsafeOptions Options {
            .NoLifetimeNull = WarnNoLifetimeNull,
            .LifetimeDisabled = WarnLifetimeDisabled,
            .LifetimeGlobal = WarnLifetimeGlobal,
            .LifetimeOutput = WarnLifetimeOutput,
        };
        return std::make_unique<AstConsumer>(Options);
    }
};

int main(int argc, const char** argv)
{
    const cl::extrahelp CommonHelp(CommonOptionsParser::HelpMessage);
    const cl::extrahelp CppSafeHelp(R"(
Lifetime checks:
    default: local pset check, enforce function preconditions
)");

    const llvm::InitLLVM _(argc, argv);

    auto OptionsParser = CommonOptionsParser::create(argc, argv, CppSafeCategory);
    if (!OptionsParser) {
        llvm::WithColor::error() << llvm::toString(OptionsParser.takeError());
        return 1;
    }

    ClangTool Tool(OptionsParser->getCompilations(), OptionsParser->getSourcePathList());
    return Tool.run(newFrontendActionFactory<LifetimeFrontendAction>().get());
}
