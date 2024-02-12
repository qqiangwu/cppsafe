struct Elem {};

struct Value
{
    Elem x;

    Value();
    ~Value();
};

template <class T> void __lifetime_pset(T&&);

void test(int n)
{
    for (int i = 0; i < n; ++i) {
        Value v;

        __lifetime_pset(v);   // expected-warning {{pset(v) = (v)}}
        __lifetime_pset(v.x); // expected-warning {{pset(v.x) = ((global))}}
        v.x = Elem{};
        __lifetime_pset(v.x); // expected-warning {{pset(v.x) = (v.x)}}
    }
}