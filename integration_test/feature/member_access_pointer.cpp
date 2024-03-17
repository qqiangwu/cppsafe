struct [[gsl::Owner()]] Owner {};

struct [[gsl::Pointer()]] Ptr
{
    Owner o;
    int i;
    int* p;
};

template <class T> void __lifetime_pset(T&&);

void __lifetime_pmap();

void foo(Ptr p)
{
    __lifetime_pset(p);  // expected-warning {{pset(p) = (*p)}}
    __lifetime_pset(p.o);  // expected-warning {{pset(p.o) = (*(*p).o)}}
    __lifetime_pset(p.i);  // expected-warning {{pset(p.i) = ((*p).i)}}
    __lifetime_pset(p.p);  // expected-warning {{((global))}}
}

struct Test
{
    Ptr p;

    void foo()
    {
        __lifetime_pset(p);  // expected-warning {{pset(p) = (*(*this).p)}}
        __lifetime_pset(p.o);  // expected-warning {{pset(p.o) = (*(*(*this).p).o)}}
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
    __lifetime_pset(p->o);  // expected-warning {{pset(p->o) = ((**o).o)}}
    __lifetime_pset(p->i);  // expected-warning {{pset(p->i) = ((**o).i)}}
    __lifetime_pset(p->p);  // expected-warning {{((global))}}
}

struct Test2
{
    Owner2 o;

    void foo()
    {
        __lifetime_pmap();
        __lifetime_pset(o);  // expected-warning {{(*(*this).o)}}

        auto p = o.Get();
        __lifetime_pset(p);  // expected-warning {{pset(p) = (*(*this).o)}}
        __lifetime_pset(p->o);  // expected-warning {{pset(p->o) = ((*(*this).o).o)}}
        __lifetime_pset(p->i);  // expected-warning {{pset(p->i) = ((*(*this).o).i)}}
        __lifetime_pset(p->p);  // expected-warning {{((global))}}
    }
};
