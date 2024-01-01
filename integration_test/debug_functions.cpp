struct Dummy
{
    Dummy(int x, int y);

    Dummy(int x, int y, int* z);

    int Foo();

    int Foo(int* z);

    void Bar(int* z);
};

int Foo(int* z);

template <class T>
void __lifetime_contracts(T&&) {}

void __lifetime_pmap();

void test_lifetime_contracts()
{
    __lifetime_contracts(Dummy(0, 0));  // expected-warning {{pset(Pre(this)) = (*this)}}
    __lifetime_contracts(Dummy(0, 0, nullptr));
    // expected-warning@-1 {{pset(Pre(this)) = (*this)}}
    // expected-warning@-2 {{pset(Pre(z)) = ((null), *z)}}

    Dummy d{0, 0};
    __lifetime_contracts(d.Foo());  // expected-warning {{pset(Pre(this)) = (*this)}}
    __lifetime_contracts(d.Foo(nullptr));
    // expected-warning@-1 {{pset(Pre(this)) = (*this)}}
    // expected-warning@-2 {{pset(Pre(z)) = ((null), *z)}}

    __lifetime_contracts(Foo(nullptr));
    // expected-warning@-1 {{pset(Pre(z)) = ((null), *z)}}

    __lifetime_contracts([] {
        Dummy d(0, 0);
        d.Bar(nullptr);
    });
    // expected-warning@-4 {{pset(Pre(this)) = (*this)}}
    // expected-warning@-5 {{pset(Pre(z)) = ((null), *z)}}
}

#if 0
struct Agg
{
    int v;
    int* p;
    int* q;
};

struct NonAgg
{
    int* p;
    double* q;

    ~NonAgg();

    Agg test(Agg a, Agg& b, int& p)
    {
        __lifetime_pmap();
        __lifetime_contracts(&NonAgg::test);
        return {};
    }
};
#endif