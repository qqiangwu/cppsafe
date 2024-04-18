#include "../feature/common.h"

struct Inner {
    int* m;
};

struct A {
    int* x;
    double* y;
    Inner i;
};

A foo(int* x);

void test_contract_return()
{
    __lifetime_contracts(&foo);
    // expected-warning@-1 {{pset(Pre(x)) = ((null), *x)}}
    // expected-warning@-2 {{pset(Post((return value).x)) = ((null), *x)}}
    // expected-warning@-3 {{pset(Post((return value).y)) = ((global), (null))}}
    // expected-warning@-4 {{pset(Post((return value).i.m)) = ((null), *x)}}

    int x = 0;
    A m = foo(&x);
    __lifetime_pset(m);  // expected-warning {{pset(m) = (m)}}
    __lifetime_pset(m.x);  // expected-warning {{pset(m.x) = (x)}}
    __lifetime_pset(m.y);  // expected-warning {{pset(m.y) = ((global))}}
    __lifetime_pset(m.i);  // expected-warning {{pset(m.i) = (m.i)}}
    __lifetime_pset(m.i.m);  // expected-warning {{pset(m.i.m) = (x)}}
}