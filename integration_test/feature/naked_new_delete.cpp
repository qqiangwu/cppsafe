// ARGS: -Wlifetime-disabled

template <class T>
void __lifetime_pset(T&&) {}

void foo()
{
    int* p = new int{};  // expected-warning {{naked new-deletes disables lifetime analysis}}
    __lifetime_pset(p);  // expected-warning {{pset(p) = ((global))}}

    delete p;
    __lifetime_pset(p);  // expected-warning {{pset(p) = ((invalid))}}
}