#include "../feature/common.h"

template <class T>
struct span
{
    using iterator = T*;
    typedef const T* const_iterator;
};

using A CPPSAFE_NONNULL = int*;
typedef int* B CPPSAFE_NONNULL;

void foo(span<int>::iterator it1, span<int>::const_iterator it2)
{
    __lifetime_contracts(&foo);
    // expected-warning@-1 {{(*it1)}}
    // expected-warning@-2 {{(*it2)}}
}

void bar(A a, B b)
{
    __lifetime_contracts(&bar);
    // expected-warning@-1 {{(*a)}}
    // expected-warning@-2 {{(*b)}}
}

struct [[gsl::Pointer(int)]] NonNullPtr
{
};

struct [[gsl::Pointer(int)]] NullablePtr
{
    operator bool();
};

void coo(NonNullPtr p1, NullablePtr p2)
{
    __lifetime_contracts(&coo);
    // expected-warning@-1 {{(*p1)}}
    // expected-warning@-2 {{((null), *p2)}}
}

void d1()
{
    NonNullPtr p1;
    NullablePtr p2;
    span<int>::iterator p3;
    A p4;
    B p5;

    __lifetime_pset(p1);  // expected-warning {{= ((global))}}
    __lifetime_pset(p2);  // expected-warning {{pset(p2) = ((null))}}
    __lifetime_pset(p3);  // expected-warning {{((invalid))}}
    __lifetime_pset(p4);  // expected-warning {{((invalid))}}
    __lifetime_pset(p5);  // expected-warning {{((invalid))}}
}

void d2()
{
    NonNullPtr p1{};
    NullablePtr p2{};
    span<int>::iterator p3{};  // expected-warning {{assigning a null pointer to a non-null object}}
    A p4{};  // expected-warning {{assigning a null pointer to a non-null object}}
    B p5{};  // expected-warning {{assigning a null pointer to a non-null object}}

    __lifetime_pset(p1);  // expected-warning {{pset(p1) = ((global))}}
    __lifetime_pset(p2);  // expected-warning {{pset(p2) = ((null))}}
    __lifetime_pset(p3);  // expected-warning {{pset(p3) = ((null))}}
    __lifetime_pset(p4);  // expected-warning {{pset(p4) = ((null))}}
    __lifetime_pset(p5);  // expected-warning {{pset(p5) = ((null))}}
}

void pass_nonnull_primitive_to_fun()
{
    A a{};  // expected-note {{default-constructed pointers are assumed to be null}}
    // expected-note@-1 {{assigned here}}
    // expected-warning@-2 {{assigning a null pointer to a non-null object}}
    B b{};  // expected-note {{default-constructed pointers are assumed to be null}}
    // expected-note@-1 {{assigned here}}
    // expected-warning@-2 {{assigning a null pointer to a non-null object}}
    span<int>::iterator p{};  // expected-note 2 {{default-constructed pointers are assumed to be null}}
    // expected-note@-1 2 {{assigned here}}
    // expected-warning@-2 {{assigning a null pointer to a non-null object}}
    NonNullPtr p1{};
    NullablePtr p2{};

    bar(a, b);  // expected-warning 2 {{passing a null pointer as argument where a non-null pointer is expected}}
    foo(p, p);  // expected-warning 2 {{passing a null pointer as argument where a non-null pointer is expected}}
    coo(p1, p2);
}