// ARGS: --Wlifetime-disabled

#include "../feature/common.h"

void free(int* p)
{
    // expected-note@+1 {{deleted here}}
    delete p;  // expected-warning {{naked new-deletes disables lifetime analysis}}
    __lifetime_pset(p);  // expected-warning {{pset(p) = ((invalid))}}
}  // expected-warning {{returning a dangling pointer as output value '*p'}}

void free2(void* p)
{
    // expected-note@+1 {{deleted here}}
    delete (int*)p;  // expected-warning {{naked new-deletes disables lifetime analysis}}
    // expected-warning@-1 {{unsafe cast disables lifetime analysis}}
    __lifetime_pset(p);  // expected-warning {{pset(p) = ((invalid))}}
}  // expected-warning {{returning a dangling pointer as output value '*p'}}

void free3(void* p)
{
    // expected-note@+1 {{deleted here}}
    delete static_cast<int*>(p);  // expected-warning {{naked new-deletes disables lifetime analysis}}
    // expected-warning@-1 {{unsafe cast disables lifetime analysis}}
    __lifetime_pset(p);  // expected-warning {{pset(p) = ((invalid))}}
}  // expected-warning {{returning a dangling pointer as output value '*p'}}

void free4(void* p)
{
    auto* t = (int*)p;
    // expected-warning@-1 {{unsafe cast disables lifetime analysis}}
    // expected-note@+1 {{deleted here}}
    delete t;  // expected-warning {{naked new-deletes disables lifetime analysis}}
    __lifetime_pset(p);  // expected-warning {{pset(p) = ((invalid))}}
}  // expected-warning {{returning a dangling pointer as output value '*p'}}

CPPSAFE_POST("*p", ":invalid")
void free5(int* p)
{
    delete p;  // expected-warning {{naked new-deletes disables lifetime analysis}}
}