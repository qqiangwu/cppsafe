#include "../feature/common.h"

CPPSAFE_CAPTURE("p")  // expected-warning {{this pre/postcondition is not supported}}
void foo(int* p);

struct Aggr {
    CPPSAFE_CAPTURE("p")  // expected-warning {{this pre/postcondition is not supported}}
    void foo(int* p) {}
};

struct [[gsl::Owner]] MyOwner {
    CPPSAFE_CAPTURE("p")  // expected-warning {{this pre/postcondition is not supported}}
    void foo(int* p) {}
};

struct MyValue {
    CPPSAFE_CAPTURE("p")  // expected-warning {{this pre/postcondition is not supported}}
    void foo(int* p) {}
};

struct [[gsl::Pointer]] MyPtr {
    CPPSAFE_CAPTURE("p", "q")
    void foo(int* p, double* q, int* t) {
        __lifetime_contracts(&MyPtr::foo);
        // expected-warning@-1 {{pset(Pre(this)) = (*this)}}
        // expected-warning@-2 {{pset(Pre(*this)) = (**this)}}
        // expected-warning@-3 {{pset(Pre(p)) = ((null), *p)}}
        // expected-warning@-4 {{pset(Pre(q)) = ((null), *q)}}
        // expected-warning@-5 {{pset(Pre(t)) = ((null), *t)}}
        // expected-warning@-6 {{pset(Post(*this)) = (**this, *p, *q)}}
    }
};

void test_owner()
{
    MyOwner o;
    int p;
    o.foo(&p);

    foo(&p);
}