template <class T>
void __lifetime_pset(T&&);

void foo()
{
    int i;
    auto* p = __builtin_addressof(i);
    __lifetime_pset(p);  // expected-warning {{pset(p) = (i)}}
}