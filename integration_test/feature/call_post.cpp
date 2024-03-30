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

struct MyPair {
    int x;
    Owner<int> y;
};

CPPSAFE_POST("return", "*p")
const Owner<int>& get(const Owner<MyPair>& p)
{
    return p.get().y;
};

CPPSAFE_POST("return", "*p")
const Owner<int>& get2(const Owner<Ptr<Owner<int>>>& p)
{
    __lifetime_contracts(&get2);
    // expected-warning@-1 {{pset(Pre(p)) = (*p)}}
    // expected-warning@-2 {{pset(Pre(*p)) = (**p)}}
    // expected-warning@-3 {{pset(Post((return value))) = (**p)}}

    auto& v = p.get().get();
    __lifetime_pset(v);
    // expected-warning@-1 {{pset(v) = ((global))}}

    return v;
};
