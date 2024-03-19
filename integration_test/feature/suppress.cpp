// ARGS: --Wlifetime-null
[[gsl::suppress("lifetime")]]
void foo1(int* p)
{
    *p;
}

constexpr int Global = 0;

[[clang::annotate("gsl::lifetime_pre", "p", Global)]]
void call(int* p);

#define CPPSAFE_SUPPRESS [[gsl::suppress("lifetime")]]

#define DEREF(p) *p

int foo2(int* p) // expected-note {{the parameter is assumed to be potentially null}}
{
    {
        *p;  // expected-warning {{dereferencing a possibly null pointer}}

        [[gsl::suppress("lifetime")]]
        {
            *p;
            *p;
        }
    }

    [[gsl::suppress("lifetime")]]
    int x = *p;

    [[gsl::suppress("lifetime")]] *p;

    [[gsl::suppress("lifetime")]]
    call(nullptr);

    CPPSAFE_SUPPRESS
    call(nullptr);

    [[gsl::suppress("lifetime")]]
    if (true) {
        DEREF(p);
    }

    [[gsl::suppress("lifetime")]]
    return *p;
}
