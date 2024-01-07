struct Dummy
{
    virtual ~Dummy();
};

struct [[gsl::Pointer]] TrivialDtor
{
    [[clang::annotate("gsl::lifetime_post", "*this", "p")]]
    void Set(Dummy* p);

    ~TrivialDtor();
};

struct [[gsl::Pointer]] NonTrivialDtor
{
    [[clang::annotate("gsl::lifetime_post", "*this", "p")]]
    void Set(Dummy* p);

    [[clang::annotate("gsl::lifetime_pre", "this", "*this")]]
    ~NonTrivialDtor();
};

void test_trivial_dtor()
{
    TrivialDtor d;

    Dummy m;
    d.Set(&m);
}

void test_non_trival_dtor()
{
    NonTrivialDtor d;

    Dummy m;
    d.Set(&m);
}
// expected-warning@-1 {{dereferencing a dangling pointer}}
// expected-note@-2 {{pointee 'm' left the scope here}}