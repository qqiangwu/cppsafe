#include "cppsafe/AstConsumer.h"
#include "cppsafe/Options.h"

#include <cctype>
#include <clang/AST/ASTConsumer.h>
#include <clang/Frontend/CompilerInstance.h>
#include <clang/Frontend/FrontendAction.h>
#include <clang/Lex/HeaderSearchOptions.h>
#include <clang/Tooling/ArgumentsAdjusters.h>
#include <clang/Tooling/CommonOptionsParser.h>
#include <clang/Tooling/Tooling.h>
#include <cpp-subprocess/subprocess.hpp>
#include <fmt/core.h>
#include <llvm/ADT/StringExtras.h>
#include <llvm/ADT/StringRef.h>
#include <llvm/ADT/iterator_range.h>
#include <llvm/Support/CommandLine.h>
#include <llvm/Support/Error.h>
#include <llvm/Support/InitLLVM.h>
#include <llvm/Support/WithColor.h>

#include <algorithm>
#include <cstdlib>
#include <functional>
#include <memory>
#include <stdexcept>
#include <string>
#include <vector>

using namespace clang::tooling;
using namespace llvm;
using namespace cppsafe;

static cl::desc desc(StringRef Description) { return { Description.ltrim() }; }

// NOLINTNEXTLINE(cppcoreguidelines-avoid-non-const-global-variables)
static cl::OptionCategory CppSafeCategory("cppsafe options");

static const cl::opt<bool> WarnNoLifetimeNull("Wno-lifetime-null",
    desc("Disable flow-sensitive warnings about nullness of Pointers"), cl::init(false), cl::cat(CppSafeCategory));

static const cl::opt<bool> WarnNoLifetimePost(
    "Wno-lifetime-post", desc("Disable post-condition check"), cl::init(false), cl::cat(CppSafeCategory));

static const cl::opt<bool> WarnLifetimeGlobal("Wlifetime-global",
    desc("Flow-sensitive analysis that requires global variables of Pointer type to always point to variables with "
         "static lifetime"),
    cl::init(false), cl::cat(CppSafeCategory));

static const cl::opt<bool> WarnLifetimeDisabled("Wlifetime-disabled",
    desc("Get warnings when the flow-sensitive analysis is disabled on a function due to forbidden constructs"),
    cl::init(false), cl::cat(CppSafeCategory));

static const cl::opt<bool> WarnLifetimeOutput("Wlifetime-output",
    desc("Enforce output parameter validity check in all paths"), cl::init(false), cl::cat(CppSafeCategory));

struct DetectSystemIncludesError : public std::runtime_error {
    using std::runtime_error::runtime_error;
};

class LifetimeFrontendAction : public clang::ASTFrontendAction {
public:
    LifetimeFrontendAction()
    try {
        using namespace subprocess;

        const auto* Cxx = std::invoke([] {
            // NOLINTNEXTLINE(concurrency-mt-unsafe): used only once
            if (const auto* Cxx = std::getenv("CXX")) {
                return Cxx;
            }

            return "c++";
        });

        auto P = Popen(fmt::format("{} -E -xc++ -Wp,-v -", Cxx), input { PIPE }, output { PIPE }, error { PIPE });
        auto Out = P.communicate("", 0).second;
        if (P.retcode() != 0) {
            throw DetectSystemIncludesError(Out.string());
        }

        const auto Lines = Out.string();
        auto R = split(Lines, '\n');
        auto It = std::find_if(R.begin(), R.end(), [](const auto& L) { return L.startswith("#include <"); });
        if (It == R.end() || ++It == R.end()) {
            throw DetectSystemIncludesError("empty include directories");
        }

        for (auto L : make_range(It, R.end())) {
            if (L.starts_with("End of search list")) {
                break;
            }
            if (L.contains("(framework directory)")) {
                continue;
            }

            SystemIncludes.push_back(L.drop_while([](const char C) { return std::isspace(C); }).str());
        }
    } catch (const subprocess::CalledProcessError& E) {
        throw DetectSystemIncludesError(E.what());
    }

    bool PrepareToExecuteAction(clang::CompilerInstance& CI) override
    {
        auto& Opts = CI.getHeaderSearchOpts();
        if (Opts.UseStandardSystemIncludes) {
            for (const auto& D : SystemIncludes) {
                Opts.AddPath(D, clang::frontend::IncludeDirGroup::System, false, true);
            }
        }
        return true;
    }

    std::unique_ptr<clang::ASTConsumer> CreateASTConsumer(clang::CompilerInstance&, llvm::StringRef) override
    {
        const CppsafeOptions Options {
            .NoLifetimeNull = WarnNoLifetimeNull,
            .NoLifetimePost = WarnNoLifetimePost,
            .LifetimeDisabled = WarnLifetimeDisabled,
            .LifetimeGlobal = WarnLifetimeGlobal,
            .LifetimeOutput = WarnLifetimeOutput,
        };

        return std::make_unique<AstConsumer>(Options);
    }

private:
    std::vector<std::string> SystemIncludes;
};

int main(int argc, const char** argv)
{
    const cl::extrahelp CommonHelp(CommonOptionsParser::HelpMessage);
    const cl::extrahelp CppSafeHelp(R"(
Extra args:
    cppsafe <cppsafe options> -- <compiler options>
    cppsafe --Wno-lifetime-null -- -std=c++20 -Wno-unused
)");

    const llvm::InitLLVM _(argc, argv);

    const char* Overview = "C++ Core Guidelines Lifetime profile static analyzer";
    auto OptionsParser = CommonOptionsParser::create(argc, argv, CppSafeCategory, llvm::cl::OneOrMore, Overview);
    if (!OptionsParser) {
        llvm::WithColor::error() << llvm::toString(OptionsParser.takeError());
        return 1;
    }

    ClangTool Tool(OptionsParser->getCompilations(), OptionsParser->getSourcePathList());

    Tool.appendArgumentsAdjuster([](CommandLineArguments Args, StringRef) {
        Args.push_back(fmt::format("-D__CPPSAFE__={}", CPPSAFE_VERSION));
        return Args;
    });

    try {
        return Tool.run(newFrontendActionFactory<LifetimeFrontendAction>().get());
    } catch (const DetectSystemIncludesError& E) {
        llvm::WithColor::error() << "Cannot find standard includes:" << E.what();
    }

    return EXIT_FAILURE;
}
