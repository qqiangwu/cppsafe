#include "../feature/common.h"

int* f(int* m);

void test_basic()
{
    int x = 0;
    auto* p = f(&x);
    __lifetime_pset(p);  // expected-warning {{pset(p) = (x)}}
}

void test_invalid()
{
    int* p;  // expected-note {{it was never initialized here}}
    int* a = f(p);  // expected-warning {{passing a dangling pointer as argument}}
    __lifetime_pset(a);  // expected-warning {{pset(a) = ((invalid))}}
}