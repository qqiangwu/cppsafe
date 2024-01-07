struct [[gsl::Owner(int)]] Dummy
{
    int* Get();

    [[clang::annotate("gsl::lifetime_const")]] void Foo();
};

void Foo(Dummy& p [[clang::annotate("gsl::lifetime_const")]]);

template <class T>
void __lifetime_pset(T&&);

void test_lifetime_const()
{
    Dummy d;
    int* p = d.Get();

    Foo(d);
    __lifetime_pset(p);  // expected-warning {{pset(p) = (*d)}}

    d.Foo();
    __lifetime_pset(p);  // expected-warning {{pset(p) = (*d)}}
}