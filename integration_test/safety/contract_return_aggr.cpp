#include "../feature/common.h"

struct Inner {
    int* m;
};

struct A {
    int* x;
    double* y;
    Inner i;
};

struct B {
    int* x;
};

A foo(int* x);
B get();

void test_contract_return()
{
    __lifetime_contracts(&foo);
    // expected-warning@-1 {{pset(Pre(x)) = ((null), *x)}}
    // expected-warning@-2 {{pset(Post((return value).x)) = ((null), *x)}}
    // expected-warning@-3 {{pset(Post((return value).y)) = ((global), (null))}}
    // expected-warning@-4 {{pset(Post((return value).i.m)) = ((null), *x)}}

    int x = 0;
    A m = foo(&x);
    __lifetime_pset(m);  // expected-warning {{pset(m) = (m)}}
    __lifetime_pset(m.x);  // expected-warning {{pset(m.x) = (x)}}
    __lifetime_pset(m.y);  // expected-warning {{pset(m.y) = ((global))}}
    __lifetime_pset(m.i);  // expected-warning {{pset(m.i) = (m.i)}}
    __lifetime_pset(m.i.m);  // expected-warning {{pset(m.i.m) = (x)}}

    A n = foo(get().x);
    __lifetime_pset(n);  // expected-warning {{pset(n) = (n)}}
    __lifetime_pset(n.x);  // expected-warning {{pset(n.x) = ((global))}}
    __lifetime_pset(n.y);  // expected-warning {{pset(n.y) = ((global))}}
    __lifetime_pset(n.i);  // expected-warning {{pset(n.i) = (n.i)}}
    __lifetime_pset(n.i.m);  // expected-warning {{pset(n.i.m) = ((global))}}

    int y = 0;
    m = foo(&y);
    __lifetime_pset(m);  // expected-warning {{pset(m) = (m)}}
    __lifetime_pset(m.x);  // expected-warning {{pset(m.x) = (y)}}
    __lifetime_pset(m.y);  // expected-warning {{pset(m.y) = ((global))}}
    __lifetime_pset(m.i);  // expected-warning {{pset(m.i) = (m.i)}}
    __lifetime_pset(m.i.m);  // expected-warning {{pset(m.i.m) = (y)}}

    n = foo(B{&y}.x);
    __lifetime_pset(n);  // expected-warning {{pset(n) = (n)}}
    __lifetime_pset(n.x);  // expected-warning {{pset(n.x) = (y)}}
    __lifetime_pset(n.y);  // expected-warning {{pset(n.y) = ((global))}}
    __lifetime_pset(n.i);  // expected-warning {{pset(n.i) = (n.i)}}
    __lifetime_pset(n.i.m);  // expected-warning {{pset(n.i.m) = (y)}}

    n = foo(B{}.x);
    __lifetime_pset(n);  // expected-warning {{pset(n) = (n)}}
    __lifetime_pset(n.x);  // expected-warning {{pset(n.x) = ((global))}}
    __lifetime_pset(n.y);  // expected-warning {{pset(n.y) = ((global))}}
    __lifetime_pset(n.i);  // expected-warning {{pset(n.i) = (n.i)}}
    __lifetime_pset(n.i.m);  // expected-warning {{pset(n.i.m) = ((global))}}

    int* z = B{}.x;
    __lifetime_pset(z);  // expected-warning {{pset(z) = ((null))}}
}

B createB()
{
    const B b { .x = nullptr };
    return b;
}

B createStaticB()
{
    static int t = 0;
    static const B b { .x = &t };
    __lifetime_pmap();
    return b;
}

struct [[gsl::Owner(char)]] String {};
struct [[gsl::Pointer(char)]] StringView {
    StringView(const String&);

    void Foo();
};

struct Wrapper {
    StringView s1;
    StringView s2;
};

Wrapper get(StringView);

void test_string_view()
{
    auto s = get(String{});  // expected-note {{temporary was destroyed at the end of the full expression}}
    __lifetime_pset(s.s1);  // expected-warning {{pset(s.s1) = ((invalid))}}
    __lifetime_pset(s.s2);  // expected-warning {{pset(s.s2) = ((invalid))}}
    s.s1.Foo();  // expected-warning {{passing a dangling pointer as argument}}
}