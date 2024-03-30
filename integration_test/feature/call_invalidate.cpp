#include "common.h"

void test_subobject()
{
    Owner<Pair<Owner<int>, Owner<int>>> o;

    auto& it = o.get();
    __lifetime_pset(it);  // expected-warning {{pset(it) = (*o)}}
    __lifetime_pset(it.first);  // expected-warning {{pset(it.first) = (*(*o).first)}}

    [](auto& o){}(o);  // expected-note {{modified here}}
    [](auto o){}(it.first);  // expected-warning {{dereferencing a dangling pointer}}

    auto& it2 = o.get();
    __lifetime_pset(it2);  // expected-warning {{ = (*o)}}
    __lifetime_pset(it2.first);  // expected-warning {{ = (*(*o).first)}}
}

struct Aggr {
    Owner<int> a;
};

void test_value_subobject()
{
    // TODO
    Aggr o;
    auto& s = o.a;
    __lifetime_pset(s); // expected-warning {{ = (o.a)}}

    [](auto& o){}(o);
    __lifetime_pset(s); // expected-warning {{ = (o.a)}}

    auto& p = o.a;
    __lifetime_pset(p); // expected-warning {{ = (o.a)}}
}
