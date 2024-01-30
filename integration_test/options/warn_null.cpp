// ARGS: --Wno-lifetime-null

// expected-no-diagnostics
template <class T>
using not_null = T;

void foo1()
{
    not_null<int*> p = nullptr;
}

void foo2(bool c)
{
    int* p = c? new int{}: nullptr;

    if (c) {
        *p;
    }
}

not_null<int*> foo3()
{
    return nullptr;
}

void foo4(int* p)
{
    *p;
}