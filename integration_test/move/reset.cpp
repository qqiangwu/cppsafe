// ARGS: --Wno-lifetime-null

template <class T> void __lifetime_pset(T&&);

namespace std {

template <class T> class [[gsl::Owner(int)]] unique_ptr {
public:
    void reset();
};

}

void foo(std::unique_ptr<int>* p)
{
    __lifetime_pset(*p);  // expected-warning {{(**p)}}
    (*p).reset();
    __lifetime_pset(*p);  // expected-warning {{(**p)}}
}

void foo2(std::unique_ptr<int>* p)
{
    __lifetime_pset(*p);  // expected-warning {{(**p)}}
    p->reset();
    __lifetime_pset(*p);  // expected-warning {{(**p)}}
}

void bar(std::unique_ptr<int>& p)
{
    __lifetime_pset(p);  // expected-warning {{(**p)}}
    p.reset();
    __lifetime_pset(p);  // expected-warning {{(**p)}}
}

void coo(std::unique_ptr<int> p)
{
    __lifetime_pset(p);  // expected-warning {{(*p)}}
    p.reset();
    __lifetime_pset(p);  // expected-warning {{(*p)}}
}

struct Value
{
    void reset();

    void test()
    {
        __lifetime_pset(*this);  // expected-warning {{(unknown)}}
        reset();
        __lifetime_pset(*this);  // expected-warning {{(*this)}}
    }
};