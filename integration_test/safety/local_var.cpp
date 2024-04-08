#include "../feature/common.h"

struct Aggr {
    int x;
    int* y;
    Ptr<int> z;
};

struct Aggr2 {
    Aggr a;
    Aggr* b;
};

void f1()
{
    int* p1;
    int* p2 = nullptr;

    __lifetime_pset(p1);  // expected-warning {{= ((invalid))}}
    __lifetime_pset(p2);  // expected-warning {{= ((null))}}
}

void f2()
{
    Aggr a;
    Aggr* b;
    Aggr* c = nullptr;  // expected-note 3 {{assigned here}}

    // TODO(types): finish Aggregate init
    __lifetime_pset(a);  // expected-warning {{pset(a) = (a)}}
    __lifetime_pset(a.x);  // expected-warning {{pset(a.x) = (a.x)}}
    __lifetime_pset(a.y);  // expected-warning {{pset(a.y) = ((invalid))}}
    __lifetime_pset(a.z);  // expected-warning {{pset(a.z) = ((global))}}

    __lifetime_pset(b);  // expected-warning {{pset(b) = ((invalid))}}
    __lifetime_pset(c);  // expected-warning {{pset(c) = ((null))}}

    c->x;  // expected-warning {{dereferencing a null pointer}}
    c->y;  // expected-warning {{dereferencing a null pointer}}
    c->z;  // expected-warning {{dereferencing a null pointer}}
}
