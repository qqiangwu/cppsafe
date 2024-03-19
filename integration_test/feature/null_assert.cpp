// ARGS: --Wlifetime-null

#include <cassert>

template <class T> void __lifetime_pset(T&&);

void test(bool b, int* p)
{
    __lifetime_pset(p);  // expected-warning {{((null), *p)}}

    assert(p);
    __lifetime_pset(p);  // expected-warning {{(*p)}}

    if (b) {
        p = nullptr;
    }

    if (!b) {
        assert(p);
        __lifetime_pset(p);  // expected-warning {{(*p)}}
    }

    __lifetime_pset(p);  // expected-warning {{((null), *p)}}
}