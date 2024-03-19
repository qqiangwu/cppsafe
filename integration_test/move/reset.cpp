template <class T> void __lifetime_pset(T&&);

namespace std {

template <class T> class [[gsl::Owner(int)]] unique_ptr {
public:
    void reset();
};

}

void foo(std::unique_ptr<int>* p)
{
    __lifetime_pset(*p);  // expected-warning {{pset(*p) = (**p)}}
    (*p).reset();
    __lifetime_pset(*p);  // expected-warning {{pset(*p) = (**p)}}
}

void foo2(std::unique_ptr<int>* p)
{
    __lifetime_pset(*p);  // expected-warning {{pset(*p) = (**p)}}
    p->reset();
    __lifetime_pset(*p);  // expected-warning {{pset(*p) = (**p)}}
}

void bar(std::unique_ptr<int>& p)
{
    __lifetime_pset(p);  // expected-warning {{pset(p) = (**p)}}
    p.reset();
    __lifetime_pset(p);  // expected-warning {{pset(p) = (**p)}}
}

void coo(std::unique_ptr<int> p)
{
    __lifetime_pset(p);  // expected-warning {{pset(p) = (*p)}}
    p.reset();
    __lifetime_pset(p);  // expected-warning {{pset(p) = (*p)}}
}

struct Value
{
    void reset();

    void test()
    {
        __lifetime_pset(*this);  // expected-warning {{pset(*this) = ((unknown))}}
        reset();
        __lifetime_pset(*this);  // expected-warning {{pset(*this) = (*this)}}
    }
};

struct [[gsl::Pointer(int)]] Ptr {
    void reset();
};

void foo(Ptr* p)
{
    __lifetime_pset(p);  // expected-warning {{pset(p) = (*p)}}
    __lifetime_pset(*p);  // expected-warning {{pset(*p) = (**p)}}
    p->reset();
    __lifetime_pset(p);  // expected-warning {{pset(p) = (*p)}}
    __lifetime_pset(*p);  // expected-warning {{pset(*p) = ((global))}}
}

void foo(Ptr& p)
{
    __lifetime_pset(p);  // expected-warning {{pset(p) = (**p)}}
    p.reset();
    __lifetime_pset(p);  // expected-warning {{pset(p) = ((global))}}
}

void foo(Ptr p)
{
    __lifetime_pset(p);  // expected-warning {{(*p)}}
    p.reset();
    __lifetime_pset(p);  // expected-warning {{pset(p) = ((global))}}
}