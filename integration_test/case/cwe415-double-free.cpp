#include "../feature/common.h"

void foo()
{
    int* p = new int{};

    delete p;  // expected-note {{deleted here}}
    delete p;  // expected-warning {{dereferencing a dangling pointer}}
}