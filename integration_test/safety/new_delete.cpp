#include "../feature/common.h"

void foo()
{
    auto* p = new int{};
    auto* q = p;
    delete q;
    __lifetime_pset(p);  // expected-warning {{((global))}}
    __lifetime_pset(q);  // expected-warning {{((invalid))}}
}