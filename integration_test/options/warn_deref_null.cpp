// ARGS: --Wno-lifetime-null

struct Opts {
    void Foo();
};

int* foo(Opts* p, int* q)
{
    p->Foo();
    *q = 0;
    return q;
}

int bar()
{
    int* r = foo(nullptr, nullptr);
    return *r;
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

// expected-no-diagnostics