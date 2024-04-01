#include "../feature/common.h"

int* foo1()
{
    __lifetime_contracts(&foo1);
    // expected-warning@-1 {{pset(Post((return value))) = ((global), (null))}}
    return nullptr;
}

int* foo2(int* p)
{
    __lifetime_contracts(&foo2);
    // expected-warning@-1 {{pset(Pre(p)) = ((null), *p)}}
    // expected-warning@-2 {{pset(Post((return value))) = ((null), *p)}}
    return nullptr;
}