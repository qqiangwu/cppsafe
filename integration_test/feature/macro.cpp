// expected-no-diagnostics

#ifndef __CPPSAFE__

template <class T>
void __lifetime_pset(T&&);

void foo(int* p)
{
    __lifetime_pset(p);
}

#endif