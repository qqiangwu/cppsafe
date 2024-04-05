// ARGS: -Wlifetime-null

#include "../feature/common.h"

struct Base {
    virtual ~Base();
};

struct Derived1 : Base {
    void Foo();
};

struct Derived2 : Base {
};

struct Derived11 : Derived1 {};
struct Derived12 : Derived1 {};

template <class T>
using not_null = T;

void test_dyncast_to_derived(not_null<Base*> p)
{
    auto* x = dynamic_cast<Derived1*>(p);  // expected-note {{assigned here}}
    x->Foo();  // expected-warning {{passing a possibly null pointer as argument where a non-null pointer is expected}}

    dynamic_cast<Derived1&>(*p).Foo();
}

void test_dyncast_to_base(not_null<Derived1*> p)
{
    auto* x = dynamic_cast<Base*>(p);
    __lifetime_pset(x);  // expected-warning {{pset(x) = (*p)}}
}

void test_dyncast_to_base(Derived1& p)
{
    auto& x = dynamic_cast<Base&>(p);
    __lifetime_pset(x);  // expected-warning {{pset(x) = (*p)}}
}