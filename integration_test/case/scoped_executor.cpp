#include "../feature/common.h"

struct [[gsl::Pointer]] ScopedExecutor {
    template <class Fn>
    CPPSAFE_CAPTURE("*fn")
    void Submit(Fn&& fn) {}

    [[clang::annotate("gsl::lifetime_post", "*this", ":global")]]
    void Wait();  // pset(*this) = {global}

    [[clang::annotate("gsl::lifetime_pre", "this", "*this")]]
    ~ScopedExecutor();
};

void foo(int n)
{
    ScopedExecutor ex{};

    ex.Submit([&n]{});

    int q;
    ex.Submit([&q]{});
    __lifetime_pset(ex);  // expected-warning {{pset(ex) = ((global), n, q)}}

    for (int i = 0; i < n; ++i) {
        ex.Submit([&i]{});
    }  // expected-note {{pointee 'i' left the scope here}}
}  // expected-warning {{dereferencing a dangling pointer}}

void foo2(int n)
{
    ScopedExecutor ex{};

    for (int i = 0; i < n; ++i) {
        ex.Submit([&i]{});
    }  // expected-note {{pointee 'i' left the scope here}}

    ex.Wait();  // expected-warning {{passing a dangling pointer as argument}}
}