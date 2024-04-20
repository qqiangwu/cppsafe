#include "../feature/common.h"

int* foo(int* p);

struct Base {
    virtual int* foo(int* p) = 0;
};

struct Derive : Base {
    int* foo(int* p) override;
};

void test()
{
    int x = 0;
    auto* p = foo(&x);
    __lifetime_pset(p);  // expected-warning {{pset(p) = (x)}}

    Derive d;
    auto* p2 = d.foo(&x);
    __lifetime_pset(p2);  // expected-warning {{pset(p2) = (x)}}
}