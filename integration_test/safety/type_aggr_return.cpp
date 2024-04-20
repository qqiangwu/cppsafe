#include "../feature/common.h"

struct Inner {
    int* m;
};

struct A {
    int* x;
    int y;
    Inner i;
};

A foo01()
{
    int x;
    A t{};
    t.x = &x;
    return t;  // expected-warning {{returning a dangling pointer}}
}  // expected-note@-1 {{pointee 'x' left the scope here}}

A foo02()
{
    int m;
    A t{};
    t.i.m = &m;
    return t;  // expected-warning {{returning a dangling pointer}}
}  // expected-note@-1 {{pointee 'm' left the scope here}}

A foo03()
{
    A t;  // expected-note {{it was never initialized here}}
    return t;  // expected-warning {{returning a dangling pointer}}
}

A foo04()
{
    int x;
    return {  // expected-warning {{returning a dangling pointer}}
        .x = &x,  // expected-note@-1 {{pointee 'x' left the scope here}}
    };
}

A foo05()
{
    int x;
    return A{  // expected-warning {{returning a dangling pointer}}
        .x = &x,  // expected-note@-1 {{pointee 'x' left the scope here}}
    };
}

A foo(int x)
{
    A t;
    t.x = &x;
    return t;  // expected-warning {{returning a dangling pointer}}
    // expected-note@-1 {{pointee 'x' left the scope here}}
}

struct ReturnRef {
    A member;

    const A& foo() const
    {
        return member;
    }
};

void test_member()
{
    ReturnRef ref;
    const auto& x = ref.foo();
    __lifetime_pset(x);  // expected-warning {{pset(x) = (ref)}}
}

#if 0
A* foo1()
{
    int x;
    auto* p = new A{};;
    p->x = &x;
    return p;
}

A* foo1(int x)
{
    auto* p = new A{};;
    p->x = &x;
    return p;
}
#endif