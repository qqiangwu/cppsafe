// ARGS: --Wlifetime-null --Wno-lifetime-call-null

template <class T>
using not_null = T;

// expected-no-diagnostics
void foo4(int* p)
{
    *p;
}