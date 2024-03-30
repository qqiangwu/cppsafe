// ARGS: --Wlifetime-disabled

template <typename T>
bool __lifetime_pset(const T &) { return true; }

void foo(int* p)
{
    ++p;  // expected-warning {{pointer arithmetic disables lifetime analysis}}
    __lifetime_pset(p);  // expected-warning {{pset(p) = (*p)}}
}

void bar(int* x)
{
    auto* p = x + 1;  // expected-warning {{pointer arithmetic disables lifetime analysis}}
    __lifetime_pset(p);  // expected-warning {{pset(p) = (*x)}}
}

void binop(int* x)
{
    int j = 0;
    int* q = &j;
    j++, x = q;  // expected-warning {{pointer arithmetic disables lifetime analysis}}
}
