template <class T>
void __lifetime_pset(T&&);

template <class T>
void __lifetime_contracts(T&&);

void __lifetime_pmap();

template <class T>
struct [[gsl::Owner(T)]] Owner
{
    T& get();
};

void test_ref(int n)
{
    auto& c = n;
    __lifetime_pset(c);  // expected-warning {{pset(c) = (n)}}

    auto* d = &n;
    __lifetime_pset(d);  // expected-warning {{pset(d) = (n)}}
}

void test()
{
    Owner<int> o1;

    auto& a = o1.get();
    __lifetime_pset(a);  // expected-warning {{pset(a) = (*o1)}}

    auto b = o1.get();
    __lifetime_pset(b);  // expected-warning {{pset(b) = (b)}}

    Owner<int*> o2;
    auto& c = o2.get();
    __lifetime_pset(c);  // expected-warning {{pset(c) = (*o2)}}

    auto* d = c;
    __lifetime_pset(d);  // expected-warning {{pset(d) = ((global))}}
}

struct Test
{
    Owner<int*> o;

    void test()
    {
        auto* p = o.get();
        __lifetime_pset(p);  // expected-warning {{pset(p) = (**this)}}

        auto& q = o.get();
        __lifetime_pset(q);  // expected-warning {{pset(q) = (*this)}}
    }
};