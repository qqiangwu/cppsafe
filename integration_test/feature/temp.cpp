#include "common.h"

struct ABC {
    int x;

    int* get() { return &x; }
};

void foo()
{
    auto* p = Owner<int>{}.getPtr();  // expected-note {{temporary was destroyed at the end of the full expression}}
    *p;  // expected-warning {{dereferencing a dangling pointer}}
}

void foo2()
{
    int* p = nullptr;
    p = Owner<int>{}.getPtr();
    *p;
}

void foo3()
{
    int* p = ABC{}.get();  // expected-note {{temporary was destroyed at the end of the full expression}}
    *p;  // expected-warning {{dereferencing a dangling pointer}}
}

void foo4()
{
    int* p = nullptr;
    p = ABC{}.get();
    *p;
}