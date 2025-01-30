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

int* get()
{
    static int x = 0;
    return &x;
}

int* get2()
{
    static int x;
    static int* y = &x;
    return y;
}

struct Dummy {
    int m;
};

const int* get3();
const int* get4(const Dummy& d);
const int* get5(const Owner<int>& d);
const int* get6(const Owner<Dummy>& d);
const int* get7(const Owner<Pair<int, int>>& d);

void test_contract_return()
{
    __lifetime_contracts(&get3);
    // expected-warning@-1 {{pset(Post((return value))) = ((global), (null))}}

    __lifetime_contracts(&get4);
    // expected-warning@-1 {{pset(Pre(d)) = (*d)}}
    // expected-warning@-2 {{pset(Pre((*d).m)) = (*d)}}
    // expected-warning@-3 {{pset(Post((return value))) = ((null), *d)}}

    __lifetime_contracts(&get5);
    // expected-warning@-1 {{pset(Pre(d)) = (*d)}}
    // expected-warning@-2 {{pset(Pre(*d)) = (**d)}}
    // expected-warning@-3 {{pset(Post((return value))) = ((null), **d)}}

    __lifetime_contracts(&get6);
    // expected-warning@-1 {{pset(Pre(d)) = (*d)}}
    // expected-warning@-2 {{pset(Pre(*d)) = (**d)}}
    // expected-warning@-3 {{pset(Post((return value))) = ((null), **d)}}

    __lifetime_contracts(&get7);
    // expected-warning@-1 {{pset(Pre(d)) = (*d)}}
    // expected-warning@-2 {{pset(Pre(*d)) = (**d)}}
    // expected-warning@-3 {{pset(Post((return value))) = ((null), **d)}}
}