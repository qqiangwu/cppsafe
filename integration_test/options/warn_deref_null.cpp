struct Opts {
    void Foo();
};

void foo(Opts* p, int* q)
{
    p->Foo();
    *q = 0;
}

void bar()
{
    foo(nullptr, nullptr);
}

// expected-no-diagnostics