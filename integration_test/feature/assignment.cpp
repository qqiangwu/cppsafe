template <class T>
void __lifetime_pset(T&&) {}

struct Test
{
    int* p = nullptr;
};

void test_pointer_assignment(Test* t)
{
    delete t->p;
    __lifetime_pset(t->p);  // expected-warning {{pset(t->p) = ((invalid))}}

    t = new Test{};
    __lifetime_pset(t->p);  // expected-warning {{pset(t->p) = ((global))}}
}