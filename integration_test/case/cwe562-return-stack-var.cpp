class MyClass
{
  int* m_p;

  void Foo() {  // expected-warning {{returning a dangling pointer as output value '(*this).m_p'}}
    int localVar;
    m_p = &localVar;
  }  // expected-note {{pointee 'localVar' left the scope here}}

  void Foo2()
  {  // expected-warning {{returning a dangling pointer as output value '(*this).m_p'}}
    int a[3];
    m_p = &a[0];
  }  // expected-note {{pointee 'a' left the scope here}}
};

void Get(float **x)
{
  float f;
  *x = &f;
}

struct CVariable {
  char  name[64];
};

void test(int n, char *tokens[])
{
  for (int i=0;i<n;i++)
  {
    CVariable  var;
    tokens[i]  =  var.name;
  }
}

struct Dummy {
    int* p;
};

void foo(Dummy* d)
{  // expected-warning {{returning a dangling pointer as output value '(*d).p'}}
    int x;
    d->p = &x;
}  // expected-note {{pointee 'x' left the scope here}}

void foo2(Dummy*&& d)
{
    int x;
    d->p = &x;
}

float *F()
{
  float f = 1.0;
  return &f;
  // expected-warning@-1 {{returning a dangling pointer}}
  // expected-note@-2 {{pointee 'f' left the scope here}}
}

void memcpy(void *dst, const void *src, int n);

int *Foo(bool err)
{
  int A[10];
  // ...
  if (err)
    return 0;

  int *B = new int[10];
  memcpy(B, A, sizeof(A));

  return A;
  // expected-warning@-1 {{returning a dangling pointer}}
  // expected-note@-2 {{pointee 'A' left the scope here}}
}