#include "../feature/common.h"

// TODO
// expected-no-diagnostics
class Foo {
  int *x;
  int &y;
  Foo(int a, int b);
};

Foo::Foo(int a, int b) :
  x(&a),
  y(b)
{};

struct Foo2 {
    int* x;
    int& y;

    Foo2(Owner<int> a, Owner<int> b)
        : x(a.getPtr()), y(b.get())
    {}
};