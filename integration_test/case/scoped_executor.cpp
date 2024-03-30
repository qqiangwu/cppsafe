struct [[gsl::Pointer]] ScopedExecutor {
    template <class Fn>
    [[clang::annotate("gsl::lifetime_post", "*this", "*fn")]]
    void Submit(Fn&& fn) {}  // pset(*this) += pset(*fn);

    [[clang::annotate("gsl::lifetime_post", "*this", ":global")]]
    void Wait();  // pset(*this) = {global}

    [[clang::annotate("gsl::lifetime_pre", "this", "*this")]]
    ~ScopedExecutor();
};

void foo(int n)
{
    ScopedExecutor ex{};

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