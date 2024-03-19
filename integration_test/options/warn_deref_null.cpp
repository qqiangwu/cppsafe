// ARGS: --Wlifetime-null

struct Opts {
    void Foo();
};

int* foo(Opts* p, int* q)
    // expected-note@-1 {{the parameter is assumed to be potentially null. Consider using gsl::not_null<>, a reference instead of a pointer or an assert() to explicitly remove null}}
    // expected-note@-2 {{the parameter is assumed to be potentially null. Consider using gsl::not_null<>, a reference instead of a pointer or an assert() to explicitly remove null}}
{
    p->Foo();  // expected-warning {{passing a possibly null pointer as argument where a non-null pointer is expected}}
    *q = 0;  // expected-warning {{dereferencing a possibly null pointer}}
    return q;
}

int bar()
{
    int* r = foo(nullptr, nullptr);
    // expected-note@-1 {{assigned here}}
    return *r;  // expected-warning {{dereferencing a null pointer}}
}

struct [[gsl::Pointer(int)]] Ptr {
    Ptr();
    Ptr(int* p);
};

void foo(Ptr* p);

void bar2()
{
    Ptr p;
    foo(&p);
}
