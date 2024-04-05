// ARGS: -Wlifetime-output

#include "../feature/common.h"

class Foo {
  int *x;
  int &y;
  Foo(int a, int b);

  void set(int a)
  {
    x = &a;  // expected-warning {{returning a dangling pointer as output value '(*this).x'}}
    // expected-note@-1 {{pointee 'a' left the scope here}}
  }
};

Foo::Foo(int a, int b) :
  x(&a),
  y(b)
{
}  // expected-warning {{returning a dangling pointer as output value '(*this).x'}}
   // expected-note@-1 {{pointee 'a' left the scope here}}

struct Foo2 {
    int* x;
    int& y;

    Foo2(Owner<int> a, Owner<int> b)
        : x(a.getPtr()), y(b.get())
    {
    }  // expected-warning {{returning a dangling pointer as output value '(*this).x'}}
       // expected-note@-1 {{pointee 'a' left the scope here}}

    void set(Owner<int> a)
    {
      x = a.getPtr();  // expected-warning {{returning a dangling pointer as output value '(*this).x'}}
       // expected-note@-1 {{pointee 'a' left the scope here}}
    }
};

int* get(int a)
{
  return &a;  // expected-warning {{returning a dangling pointer}}
  // expected-note@-1 {{pointee 'a' left the scope here}}
}

void test_param(int a, int** p)
{
  *p = &a;  // expected-warning {{returning a dangling pointer as output value '*p'}}
   // expected-note@-1 {{pointee 'a' left the scope here}}
}