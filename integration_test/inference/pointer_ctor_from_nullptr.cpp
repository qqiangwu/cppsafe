template <class T>
void __lifetime_pset(T&&) {}

struct [[gsl::Pointer]] Fn
{
    Fn(decltype(nullptr) p);

    void Set(decltype(nullptr) p);
};

void test()
{
    Fn f(nullptr);
    __lifetime_pset(f); // expected-warning {{pset(f) = ((null))}}

    f.Set(nullptr);
    __lifetime_pset(f); // expected-warning {{pset(f) = ((null))}}
}