// ARGS: --Wlifetime-null

template <class T>
using not_null = T;

void foo1()
{
    not_null<int*> p = nullptr;  // expected-warning {{assigning a null pointer to a non-null object}}
}

void foo2(bool c)
{
    int* p = c? new int{}: nullptr;
    // expected-note@-1 {{assigned here}}

    if (c) {
        *p;  // expected-warning {{dereferencing a possibly null pointer}}
    }
}

not_null<int*> foo3()
{
    return nullptr;  // expected-warning {{returning a null pointer where a non-null pointer is expected}}
}

void foo4(int* p)
    // expected-note@-1 {{the parameter is assumed to be potentially null. Consider using gsl::not_null<>, a reference instead of a pointer or an assert() to explicitly remove null}}
{
    *p;  // expected-warning {{dereferencing a possibly null pointer}}
}