// ARGS: -Wlifetime-null
#include "../feature/common.h"

struct A {
    int* x;
    double* y;
};

int* foo(A a);
double* bar(A a);

void test_contract_pass_by_value_null()
{
    A obj{};

    auto* x = foo(obj);
    auto* y = bar(obj);

    __lifetime_pset(x);  // expected-warning {{pset(x) = ((null))}}
    __lifetime_pset(y);  // expected-warning {{pset(y) = ((null))}}
}