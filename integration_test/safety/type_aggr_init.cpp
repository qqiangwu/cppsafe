#include "../feature/common.h"

int* getInt();

struct Aggr {
    int x;
    int* y;
    int* z = nullptr;
    int* z2 = new int{};
    int* z3 = &x;
    int* z4 = getInt();
    int* z5 = 0;
    Ptr<int> p;
    Owner<int> o;
};

struct Aggr2 {
    Aggr a;
    Aggr* b;
    Owner<int> c;
};

void test_default_init()
{
    Aggr a;
    Aggr2 b;

    __lifetime_pset(a);  // expected-warning {{pset(a) = (a)}}
    __lifetime_pset(a.x);  // expected-warning {{pset(a.x) = (a.x)}}
    __lifetime_pset(a.y);  // expected-warning {{pset(a.y) = ((invalid))}}
    __lifetime_pset(a.z);  // expected-warning {{pset(a.z) = ((null))}}
    __lifetime_pset(a.z2);  // expected-warning {{pset(a.z2) = ((global))}}
    __lifetime_pset(a.z3);  // expected-warning {{pset(a.z3) = ((unknown))}}
    __lifetime_pset(a.z4);  // expected-warning {{pset(a.z4) = ((unknown))}}
    __lifetime_pset(a.z5);  // expected-warning {{pset(a.z5) = ((null))}}
    __lifetime_pset(a.p);  // expected-warning {{pset(a.p) = ((global))}}
    __lifetime_pset(a.o);  // expected-warning {{pset(a.o) = (*a.o)}}

    __lifetime_pset(b);  // expected-warning {{pset(b) = (b)}}
    __lifetime_pset(b.a);  // expected-warning {{pset(b.a) = (b.a)}}
    __lifetime_pset(b.a.x);  // expected-warning {{pset(b.a.x) = (b.a.x)}}
    __lifetime_pset(b.a.y);  // expected-warning {{pset(b.a.y) = ((invalid))}}
    __lifetime_pset(b.a.z);  // expected-warning {{pset(b.a.z) = ((null))}}
    __lifetime_pset(b.a.z2);  // expected-warning {{pset(b.a.z2) = ((global))}}
    __lifetime_pset(b.a.z3);  // expected-warning {{pset(b.a.z3) = ((unknown))}}
    __lifetime_pset(b.a.z4);  // expected-warning {{pset(b.a.z4) = ((unknown))}}
    __lifetime_pset(b.a.z5);  // expected-warning {{pset(b.a.z5) = ((null))}}
    __lifetime_pset(b.a.p);  // expected-warning {{pset(b.a.p) = ((global))}}
    __lifetime_pset(b.a.o);  // expected-warning {{pset(b.a.o) = (*b.a.o)}}
    __lifetime_pset(b.b);  // expected-warning {{pset(b.b) = ((invalid))}}
    __lifetime_pset(b.c);  // expected-warning {{pset(b.c) = (*b.c)}}
}

void test_copy_init()
{
    int m = 0;

    Aggr2 a;
    a.a.z = &m;
    a.a.z2 = &m;
    a.a.z3 = &m;
    a.a.z4 = &m;
    a.a.z5 = &m;

    Aggr2 b = a;
    __lifetime_pset(b);  // expected-warning {{pset(b) = (b)}}
    __lifetime_pset(b.a);  // expected-warning {{pset(b.a) = (b.a)}}
    __lifetime_pset(b.a.x);  // expected-warning {{pset(b.a.x) = (b.a.x)}}
    __lifetime_pset(b.a.y);  // expected-warning {{pset(b.a.y) = ((invalid))}}
    __lifetime_pset(b.a.z);  // expected-warning {{pset(b.a.z) = (m)}}
    __lifetime_pset(b.a.z2);  // expected-warning {{pset(b.a.z2) = (m)}}
    __lifetime_pset(b.a.z3);  // expected-warning {{pset(b.a.z3) = (m)}}
    __lifetime_pset(b.a.z4);  // expected-warning {{pset(b.a.z4) = (m)}}
    __lifetime_pset(b.a.z5);  // expected-warning {{pset(b.a.z5) = (m)}}
    __lifetime_pset(b.a.p);  // expected-warning {{pset(b.a.p) = ((global))}}
    __lifetime_pset(b.a.o);  // expected-warning {{pset(b.a.o) = (*b.a.o)}}
    __lifetime_pset(b.b);  // expected-warning {{pset(b.b) = ((invalid))}}
    __lifetime_pset(b.c);  // expected-warning {{pset(b.c) = (*b.c)}}
}

void test_copy_init2()
{
    Aggr2 b = Aggr2();
    __lifetime_pset(b);  // expected-warning {{pset(b) = (b)}}
    __lifetime_pset(b.a);  // expected-warning {{pset(b.a) = (b.a)}}
    __lifetime_pset(b.a.x);  // expected-warning {{pset(b.a.x) = (b.a.x)}}
    __lifetime_pset(b.a.y);  // expected-warning {{pset(b.a.y) = ((invalid))}}
    __lifetime_pset(b.a.z);  // expected-warning {{pset(b.a.z) = ((null))}}
    __lifetime_pset(b.a.z2);  // expected-warning {{pset(b.a.z2) = ((global))}}
    __lifetime_pset(b.a.z3);  // expected-warning {{pset(b.a.z3) = ((unknown))}}
    __lifetime_pset(b.a.z4);  // expected-warning {{pset(b.a.z4) = ((unknown))}}
    __lifetime_pset(b.a.z5);  // expected-warning {{pset(b.a.z5) = ((null))}}
    __lifetime_pset(b.a.p);  // expected-warning {{pset(b.a.p) = ((global))}}
    __lifetime_pset(b.a.o);  // expected-warning {{pset(b.a.o) = (*b.a.o)}}
    __lifetime_pset(b.b);  // expected-warning {{pset(b.b) = ((invalid))}}
    __lifetime_pset(b.c);  // expected-warning {{pset(b.c) = (*b.c)}}
}

void test_copy()
{
    Aggr2 a;
    Aggr2 b;

    int m;
    int n;
    int t;
    a.a.y = &m;
    a.a.z = &n;
    a.a.z2 = &t;
    a.a.z3 = &t;
    a.a.z4 = &t;
    a.a.z5 = &t;
    a.a.p = {};

    b = a;
    __lifetime_pset(b);  // expected-warning {{pset(b) = (b)}}
    __lifetime_pset(b.a);  // expected-warning {{pset(b.a) = (b.a)}}
    __lifetime_pset(b.a.x);  // expected-warning {{pset(b.a.x) = (b.a.x)}}
    __lifetime_pset(b.a.y);  // expected-warning {{pset(b.a.y) = (m)}}
    __lifetime_pset(b.a.z);  // expected-warning {{pset(b.a.z) = (n)}}
    __lifetime_pset(b.a.z2);  // expected-warning {{pset(b.a.z2) = (t)}}
    __lifetime_pset(b.a.z3);  // expected-warning {{pset(b.a.z3) = (t)}}
    __lifetime_pset(b.a.z4);  // expected-warning {{pset(b.a.z4) = (t)}}
    __lifetime_pset(b.a.z5);  // expected-warning {{pset(b.a.z5) = (t)}}
    __lifetime_pset(b.a.p);  // expected-warning {{pset(b.a.p) = ((global))}}
    __lifetime_pset(b.a.o);  // expected-warning {{pset(b.a.o) = (*b.a.o)}}
    __lifetime_pset(b.b);  // expected-warning {{pset(b.b) = ((invalid))}}
    __lifetime_pset(b.c);  // expected-warning {{pset(b.c) = (*b.c)}}
}

void test_copy_with_init()
{
    Aggr2 b;

    int m;
    int n;
    int t;
    b.a.y = &m;
    b.a.z = &n;
    b.a.z2 = &t;
    b.a.z3 = &t;
    b.a.z4 = &t;
    b.a.z5 = &t;
    b.a.p = {};
    __lifetime_pset(b);  // expected-warning {{pset(b) = (b)}}
    __lifetime_pset(b.a);  // expected-warning {{pset(b.a) = (b.a)}}
    __lifetime_pset(b.a.x);  // expected-warning {{pset(b.a.x) = (b.a.x)}}
    __lifetime_pset(b.a.y);  // expected-warning {{pset(b.a.y) = (m)}}
    __lifetime_pset(b.a.z);  // expected-warning {{pset(b.a.z) = (n)}}
    __lifetime_pset(b.a.z2);  // expected-warning {{pset(b.a.z2) = (t)}}
    __lifetime_pset(b.a.z3);  // expected-warning {{pset(b.a.z3) = (t)}}
    __lifetime_pset(b.a.z4);  // expected-warning {{pset(b.a.z4) = (t)}}
    __lifetime_pset(b.a.z5);  // expected-warning {{pset(b.a.z5) = (t)}}
    __lifetime_pset(b.a.p);  // expected-warning {{pset(b.a.p) = ((global))}}
    __lifetime_pset(b.a.o);  // expected-warning {{pset(b.a.o) = (*b.a.o)}}
    __lifetime_pset(b.b);  // expected-warning {{pset(b.b) = ((invalid))}}
    __lifetime_pset(b.c);  // expected-warning {{pset(b.c) = (*b.c)}}
}

void test_init_init()
{
    {
        Aggr2 b = {};
        __lifetime_pset(b);  // expected-warning {{pset(b) = (b)}}
        __lifetime_pset(b.a);  // expected-warning {{pset(b.a) = (b.a)}}
        __lifetime_pset(b.a.x);  // expected-warning {{pset(b.a.x) = (b.a.x)}}
        __lifetime_pset(b.a.y);  // expected-warning {{pset(b.a.y) = ((null))}}
        __lifetime_pset(b.a.z);  // expected-warning {{pset(b.a.z) = ((null))}}
        __lifetime_pset(b.a.z2);  // expected-warning {{pset(b.a.z2) = ((global))}}
        __lifetime_pset(b.a.z3);  // expected-warning {{pset(b.a.z3) = (b.a.x)}}
        __lifetime_pset(b.a.z4);  // expected-warning {{pset(b.a.z4) = ((global))}}
        __lifetime_pset(b.a.z5);  // expected-warning {{pset(b.a.z5) = ((null))}}
        __lifetime_pset(b.a.p);  // expected-warning {{pset(b.a.p) = ((global))}}
        __lifetime_pset(b.a.o);  // expected-warning {{pset(b.a.o) = (*b.a.o)}}
        __lifetime_pset(b.b);  // expected-warning {{pset(b.b) = ((null))}}
        __lifetime_pset(b.c);  // expected-warning {{pset(b.c) = (*b.c)}}
    }

    {
        Aggr2 b{};
        __lifetime_pset(b);  // expected-warning {{pset(b) = (b)}}
        __lifetime_pset(b.a);  // expected-warning {{pset(b.a) = (b.a)}}
        __lifetime_pset(b.a.x);  // expected-warning {{pset(b.a.x) = (b.a.x)}}
        __lifetime_pset(b.a.y);  // expected-warning {{pset(b.a.y) = ((null))}}
        __lifetime_pset(b.a.z);  // expected-warning {{pset(b.a.z) = ((null))}}
        __lifetime_pset(b.a.z2);  // expected-warning {{pset(b.a.z2) = ((global))}}
        __lifetime_pset(b.a.z3);  // expected-warning {{pset(b.a.z3) = (b.a.x)}}
        __lifetime_pset(b.a.z4);  // expected-warning {{pset(b.a.z4) = ((global))}}
        __lifetime_pset(b.a.z5);  // expected-warning {{pset(b.a.z5) = ((null))}}
        __lifetime_pset(b.a.p);  // expected-warning {{pset(b.a.p) = ((global))}}
        __lifetime_pset(b.a.o);  // expected-warning {{pset(b.a.o) = (*b.a.o)}}
        __lifetime_pset(b.b);  // expected-warning {{pset(b.b) = ((null))}}
        __lifetime_pset(b.c);  // expected-warning {{pset(b.c) = (*b.c)}}
    }
}

void test_assign_with_init()
{
    int m;
    Aggr2 b;
    b.a.y = &m;
    b.a.z = &m;
    b.a.z2 = &m;
    b.a.z3 = &m;
    b.a.z4 = &m;
    b.a.z5 = &m;
    b.b = new Aggr{};

    b = {};
    __lifetime_pset(b);  // expected-warning {{pset(b) = (b)}}
    __lifetime_pset(b.a);  // expected-warning {{pset(b.a) = (b.a)}}
    __lifetime_pset(b.a.x);  // expected-warning {{pset(b.a.x) = (b.a.x)}}
    __lifetime_pset(b.a.y);  // expected-warning {{pset(b.a.y) = ((null))}}
    __lifetime_pset(b.a.z);  // expected-warning {{pset(b.a.z) = ((null))}}
    __lifetime_pset(b.a.z2);  // expected-warning {{pset(b.a.z2) = ((global))}}
    __lifetime_pset(b.a.z3);  // expected-warning {{pset(b.a.z3) = ((temporary).a.x)}}
    __lifetime_pset(b.a.z4);  // expected-warning {{pset(b.a.z4) = ((global))}}
    __lifetime_pset(b.a.z5);  // expected-warning {{pset(b.a.z5) = ((null))}}
    __lifetime_pset(b.a.p);  // expected-warning {{pset(b.a.p) = ((global))}}
    __lifetime_pset(b.a.o);  // expected-warning {{pset(b.a.o) = (*b.a.o)}}
    __lifetime_pset(b.b);  // expected-warning {{pset(b.b) = ((null))}}
    __lifetime_pset(b.c);  // expected-warning {{pset(b.c) = (*b.c)}}
}