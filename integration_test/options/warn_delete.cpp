#include "../feature/common.h"

void free(int* p)
{
    delete p;
    __lifetime_pset(p);  // expected-warning {{pset(p) = ((invalid))}}
}

void free2(void* p)
{
    delete (int*)p;
    __lifetime_pset(p);  // expected-warning {{pset(p) = ((invalid))}}
}

void free3(void* p)
{
    delete static_cast<int*>(p);
    __lifetime_pset(p);  // expected-warning {{pset(p) = ((invalid))}}
}

void free4(void* p)
{
    auto* t = (int*)p;
    delete t;
    __lifetime_pset(p);  // expected-warning {{pset(p) = ((invalid))}}
}

CPPSAFE_POST("*p", ":invalid")
void free5(int* p)
{
    delete p;
}
