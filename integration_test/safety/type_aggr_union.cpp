#include "../feature/common.h"

struct Dummy {};

struct Value {
    union {
        int a;
        Owner<Dummy> b;
    };
};

int* get1(const Value& x);
Dummy* get2(const Value& x);
const int* get3(const Value& x);

void test()
{
    Value v;

    auto* p = get1(v);
    __lifetime_pset(p);  // expected-warning {{= ((global))}}

    auto* p2 = get2(v);
    __lifetime_pset(p2);  // expected-warning {{= (v)}}

    auto* p3 = get3(v);
    __lifetime_pset(p3);  // expected-warning {{= (v)}}
}