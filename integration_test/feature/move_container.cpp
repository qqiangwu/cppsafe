// ARGS: -Wlifetime-container-move -Wlifetime-move

#include "common.h"

namespace std {

template <class T>
class [[gsl::Owner(T)]] vector {
public:
    using iterator = T*;
    using value_type = T;
    using reference_type = T&;

    iterator begin();
    iterator end();

    reference_type operator[](int x);

    reference_type back();
    void pop_back();

    ~vector();
};

}

struct Dummy {};

void test_loop()
{
    std::vector<Dummy> vec;
    __lifetime_pset(vec);  // expected-warning {{pset(vec) = (*vec)}}

    for (auto& x: vec) {
        // expected-warning@-1 {{dereferencing a possibly dangling pointer}}
        // expected-warning@+1 {{dereferencing a possibly dangling pointer}}
        auto y = std::move(x);
        // expected-note@-1 {{moved here}}
        // expected-note@-2 {{moved here}}
    }

    __lifetime_pset(vec);
    // expected-warning@-1 {{pset(vec) = (*vec)}}
    // expected-warning@-2 {{pset(vec) = ((invalid), *vec)}}
}

void test_iterator()
{
    std::vector<Dummy> vec;

    for (auto it = vec.begin(), end = vec.end(); it != end; ++it) {
        auto y = std::move(*it);
        // expected-warning@-1 {{dereferencing a possibly dangling pointer}}
        // expected-note@-2 2 {{moved here}}
        // expected-warning@-3 {{passing a possibly dangling pointer as argument}}
    }
}

void test_index()
{
    std::vector<Dummy> vec;

    for (int i = 0; i < 10; ++i) {
        auto y = std::move(vec[i]);
        // expected-warning@-1 {{use a moved-from object}}
        // expected-note@-2 {{moved here}}
    }
}

void test_back()
{
    std::vector<Dummy> vec;

    auto x = std::move(vec.back());
    // expected-note@-1 {{moved here}}

    vec.pop_back();  // expected-warning {{use a moved-from object}}
}