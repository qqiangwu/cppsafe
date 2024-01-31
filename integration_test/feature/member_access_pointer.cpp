struct [[gsl::Owner()]] Owner {};

struct [[gsl::Pointer()]] Ptr
{
    Owner o;
    int i;
    int* p;
};

template <class T> void __lifetime_pset(T&&);

void foo(Ptr p)
{
    __lifetime_pset(p);  // expected-warning {{(*p)}}
    __lifetime_pset(p.o);  // expected-warning {{((global))}}
    __lifetime_pset(p.p);  // expected-warning {{((global))}}
}

struct Test
{
    Ptr p;

    void foo()
    {
        __lifetime_pset(p);  // expected-warning {{(*this)}}
        __lifetime_pset(p.o);  // expected-warning {{(*this)}}
        __lifetime_pset(p.p);  // expected-warning {{((global))}}
    }
};

struct Pair
{
    Owner o;
    int i;
    int* p;
};

struct [[gsl::Pointer(Pair)]] Ptr2
{
    Pair* operator->();
};

struct [[gsl::Owner(Pair)]] Owner2 {
    Ptr2 Get();
};

void foo(Owner2& o)
{
    auto p = o.Get();
    __lifetime_pset(p);  // expected-warning {{(**o)}}
    __lifetime_pset(p->o);  // expected-warning {{((global))}}
    __lifetime_pset(p->i);  // expected-warning {{((global))}}
    __lifetime_pset(p->p);  // expected-warning {{((global))}}
}

struct Test2
{
    Owner2 o;

    void foo()
    {
        auto p = o.Get();
        __lifetime_pset(p);  // expected-warning {{(*this)}}
        __lifetime_pset(p->o);  // expected-warning {{(**this)}}
        __lifetime_pset(p->i);  // expected-warning {{(**this)}}
        __lifetime_pset(p->p);  // expected-warning {{((global))}}
    }
};
