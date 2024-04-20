#include "../feature/common.h"

struct Inner {
    int* m;
};

struct A {
    int* x;
    double* y;
    Owner<int>* z;
    Inner i;
};

int* foo(A a);
double* bar(A a);

void test_contract_pass_by_value()
{
    __lifetime_contracts(&foo);
    // expected-warning@-1 {{pset(Pre(a.x)) = ((null), *a.x)}}
    // expected-warning@-2 {{pset(Pre(a.y)) = ((null), *a.y)}}
    // expected-warning@-3 {{pset(Pre(a.z)) = ((null), *a.z)}}
    // expected-warning@-4 {{pset(Pre(a.i.m)) = ((null), *a.i.m)}}
    // expected-warning@-5 {{pset(Post((return value))) = ((null), *a.x)}}

    __lifetime_contracts(&bar);
    // expected-warning@-1 {{pset(Pre(a.x)) = ((null), *a.x)}}
    // expected-warning@-2 {{pset(Pre(a.y)) = ((null), *a.y)}}
    // expected-warning@-3 {{pset(Pre(a.z)) = ((null), *a.z)}}
    // expected-warning@-4 {{pset(Pre(a.i.m)) = ((null), *a.i.m)}}
    // expected-warning@-5 {{pset(Post((return value))) = ((null), *a.y)}}
}

void test_contract_pass_by_value2(int p1, double p2)
{
    A obj {
        .x = &p1,
        .y = &p2,
    };
    auto* x = foo(obj);
    auto* y = bar(obj);

    __lifetime_pset(x);  // expected-warning {{pset(x) = (p1)}}
    __lifetime_pset(y);  // expected-warning {{pset(y) = (p2)}}
}

void test_contract_pass_by_value_invalid()
{
    A obj;  // expected-note 2 {{it was never initialized here}}
    auto* x = foo(obj);  // expected-warning {{passing a dangling pointer as argument}}
    auto* y = bar(obj);  // expected-warning {{passing a dangling pointer as argument}}

    __lifetime_pset(x);  // expected-warning {{pset(x) = ((invalid))}}
    __lifetime_pset(y);  // expected-warning {{pset(y) = ((invalid))}}
}

void test_contract_pass_by_value_null()
{
    A obj{};

    auto* x = foo(obj);
    auto* y = bar(obj);

    __lifetime_pset(x);  // expected-warning {{pset(x) = ((unknown))}}
    __lifetime_pset(y);  // expected-warning {{pset(y) = ((unknown))}}
}
