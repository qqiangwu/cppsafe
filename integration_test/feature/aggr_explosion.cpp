struct Dummy {
    int* a = nullptr;
    double* b = nullptr;
};

template <class T> void __lifetime_pset(T&&);

void bar()
{
    int x{};
    double y{};
    Dummy d{};
    d.a = &x;
    d.b = &y;
    __lifetime_pset(d);  // expected-warning {{(d)}}
    __lifetime_pset(d.a);  // expected-warning {{(x)}}
    __lifetime_pset(d.b);  // expected-warning {{(y)}}
}

void foo()
{
    int x{};
    double y{};
    Dummy d{
        &x,
        &y,
    };

    __lifetime_pset(d);  // expected-warning {{(d)}}
    __lifetime_pset(d.a);  // expected-warning {{(x)}}
    __lifetime_pset(d.b);  // expected-warning {{(y)}}
}

void foo2()
{
    double y{};
    Dummy d{
        .b = &y,
    };

    __lifetime_pset(d);  // expected-warning {{(d)}}
    __lifetime_pset(d.a);  // expected-warning {{(null)}}
    __lifetime_pset(d.b);  // expected-warning {{(y)}}
}
