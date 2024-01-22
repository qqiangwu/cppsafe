// ARGS: --Wno-lifetime-post

constexpr int Return = 0;
constexpr int Global = 1;

[[clang::annotate("gsl::lifetime_post", Return, Global)]]
int& foo(int& p)
{
    return p;  // expected-warning {{returning a pointer with points-to set (*p) where points-to set ((global)) is expected}}
}

struct [[gsl::Pointer(int)]] Ptr
{
    double* m;

    double* get()
    {
        return m;
    }
};