// ARGS: --Wlifetime-disabled

#include "../feature/common.h"

void unary(int* p)
{
    auto* a = ++p;
    // expected-warning@-1 {{pointer arithmetic disables lifetime analysis}}
    auto* b = --p;
    // expected-warning@-1 {{pointer arithmetic disables lifetime analysis}}
    auto* c = p++;
    // expected-warning@-1 {{pointer arithmetic disables lifetime analysis}}
    auto* d = p--;
    // expected-warning@-1 {{pointer arithmetic disables lifetime analysis}}

    __lifetime_pset(a);  // expected-warning {{= (*p)}}
    __lifetime_pset(b);  // expected-warning {{= (*p)}}
    __lifetime_pset(c);  // expected-warning {{= (*p)}}
    __lifetime_pset(d);  // expected-warning {{= (*p)}}
}

void binary(int* p)
{
    auto* a = p + 1;
    // expected-warning@-1 {{pointer arithmetic disables lifetime analysis}}
    auto* b = 1 + p;
    // expected-warning@-1 {{pointer arithmetic disables lifetime analysis}}
    auto* c = p - 1;
    // expected-warning@-1 {{pointer arithmetic disables lifetime analysis}}

    __lifetime_pset(a);  // expected-warning {{= (*p)}}
    __lifetime_pset(b);  // expected-warning {{= (*p)}}
    __lifetime_pset(c);  // expected-warning {{= (*p)}}
}

void* get(void* p);

void foo2(int* p)
{
    void* a = p;
    // expected-warning@-1 {{unsafe cast disables lifetime analysis}}
    void* b = get(p);
    // expected-warning@-1 {{unsafe cast disables lifetime analysis}}
    void* c = nullptr;

    c = p;
    // expected-warning@-1 {{unsafe cast disables lifetime analysis}}

    __lifetime_pset(a);  // expected-warning {{= (*p)}}
    __lifetime_pset(b);  // expected-warning {{= (*p)}}
    __lifetime_pset(c);  // expected-warning {{= (*p)}}
}

void foo3(int* p)
{
    char* a = (char*)p;
    // expected-warning@-1 {{unsafe cast disables lifetime analysis}}
    char* b = nullptr;

    b = (char*)p;
    // expected-warning@-1 {{unsafe cast disables lifetime analysis}}
    __lifetime_pset(a);  // expected-warning {{= (*p)}}
    __lifetime_pset(b);  // expected-warning {{= (*p)}}
}

void foo4(int* p)
{
    char* a = reinterpret_cast<char*>(p);
    // expected-warning@-1 {{unsafe cast disables lifetime analysis}}
    char* b = nullptr;

    b = reinterpret_cast<char*>(p);
    // expected-warning@-1 {{unsafe cast disables lifetime analysis}}
    __lifetime_pset(a);  // expected-warning {{= (*p)}}
    __lifetime_pset(b);  // expected-warning {{= (*p)}}
}

void foo5(int& p)
{
    char& a = reinterpret_cast<char&>(p);
    // expected-warning@-1 {{unsafe cast disables lifetime analysis}}
    char& b = (char&)p;
    // expected-warning@-1 {{unsafe cast disables lifetime analysis}}
    __lifetime_pset(a);  // expected-warning {{= (*p)}}
    __lifetime_pset(b);  // expected-warning {{= (*p)}}
}

void new_and_delete()
{
    auto* p = new int;  // expected-warning {{naked new-deletes disables lifetime analysis}}
    __lifetime_pset(p);  // expected-warning {{pset(p) = ((global))}}

    delete p;  // expected-warning {{naked new-deletes disables lifetime analysis}}
    __lifetime_pset(p);  // expected-warning {{pset(p) = ((invalid))}}
}