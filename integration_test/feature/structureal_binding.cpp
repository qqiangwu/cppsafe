#include <tuple>

struct Dummy {
    int a = 0;
    double* b = nullptr;
};

template <class T> void __lifetime_pset(T&&) {}

Dummy get1();
Dummy& get2(Dummy&);

int main()
{
    double b{};
    Dummy d{.b = &b};

    {
        auto [x1, y1] = d;
        __lifetime_pset(x1);  // expected-warning {{(unknown)}}
        __lifetime_pset(y1);  // expected-warning {{(b)}}

        auto [x3, y3] = get1();
        __lifetime_pset(x3);  // expected-warning {{(unknown)}}
        __lifetime_pset(y3);  // expected-warning {{(unknown)}}

        auto [x4, y4] = get2(d);
        __lifetime_pset(x4);  // expected-warning {{(unknown)}}
        __lifetime_pset(y4);  // expected-warning {{(b)}}
    }

    {
        auto& [x2, y2] = d;
        __lifetime_pset(x2);  // expected-warning {{(unknown)}}
        __lifetime_pset(y2);  // expected-warning {{(b)}}

        const auto& [x3, y3] = get1();
        __lifetime_pset(x3);  // expected-warning {{(unknown)}}
        __lifetime_pset(y3);  // expected-warning {{(unknown)}}

        auto& [x4, y4] = get2(d);
        __lifetime_pset(x4);  // expected-warning {{(unknown)}}
        __lifetime_pset(y4);  // expected-warning {{(b)}}
    }
}

std::tuple<int*, double*> get3();

void test_tuple()
{
    int x;
    double y;
    std::tuple<int*, double*> t(&x, &y);

    {
        auto [a, b] = t;
        __lifetime_pset(a);  // expected-warning {{((global))}}
        __lifetime_pset(b);  // expected-warning {{((global))}}
        __lifetime_pset(t);  // expected-warning {{(t)}}
    }

    {
        auto& [a, b] = t;
        __lifetime_pset(a);  // expected-warning {{((global))}}
        __lifetime_pset(b);  // expected-warning {{((global))}}
        __lifetime_pset(t);  // expected-warning {{(t)}}
    }

    {
        auto [a, b] = get3();
        __lifetime_pset(a);  // expected-warning {{((global))}}
        __lifetime_pset(b);  // expected-warning {{((global))}}
    }
}
