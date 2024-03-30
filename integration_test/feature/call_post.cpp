#include "common.h"

void foo(int** p);

struct Test {

int* p;

void test()
{
    delete p;
    __lifetime_pset(p);  // expected-warning {{pset(p) = ((invalid))}}

    foo(&p);
    __lifetime_pset(p);  // expected-warning {{pset(p) = ((global))}}
}

};

void alloc(void** p);

void test_alloc()
{
    void* a;
    int* b;

    alloc(&a);
    foo(&b);

    __lifetime_pset(a);  // expected-warning {{pset(a) = (a)}}
    __lifetime_pset(b);  // expected-warning {{pset(b) = ((global))}}
}

struct Value {
    ~Value();
};

void foo(Value** v)
{
    if (v == nullptr) {
        return;
    }
    *v = new Value{};
}

struct Pair {
    int x;
    Owner<int> y;
};

CPPSAFE_POST("return", "*p")
const Owner<int>& get(const Owner<Pair>& p)
{
    return p.get().y;
};
