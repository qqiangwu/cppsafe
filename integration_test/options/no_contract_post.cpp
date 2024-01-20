// ARGS: --Wno-lifetime-post

constexpr int Return = 0;
constexpr int Global = 1;

// expected-no-diagnostics
[[clang::annotate("gsl::lifetime_post", Return, Global)]]
int& foo(int& p)
{
    return p;
}