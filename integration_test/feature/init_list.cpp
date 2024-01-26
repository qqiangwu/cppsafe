struct [[gsl::Pointer(int)]] Ptr {
    Ptr(const Ptr& p);
};

struct [[gsl::Owner(int)]] Owner {
    operator Ptr();
};

Owner get();

template <class T>
void __lifetime_pset(T&&);

void foo()
{
    Ptr p2{get()};
    __lifetime_pset(p2);  // expected-warning {{((invalid))}}
}