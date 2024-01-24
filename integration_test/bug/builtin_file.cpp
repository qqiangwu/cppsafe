template <class T>
void __lifetime_pset(T&&);

void foo()
{
    auto* p = __builtin_FILE();
    __lifetime_pset(p);  // expected-warning {{pset(p) = ((global))}}
}