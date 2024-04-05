template <class T>
void __lifetime_pset(T&&) {}

struct Test
{
    int* p = nullptr;
};

void test_pointer_assignment(Test* t)
{
    delete t->p;  // expected-note {{deleted here}}
    __lifetime_pset(t->p);  // expected-warning {{pset(t->p) = ((invalid))}}

    t = new Test{};
    __lifetime_pset(t->p);  // expected-warning {{pset(t->p) = ((global))}}
    // expected-warning@-1 {{returning a dangling pointer as output value '(*t).p'}}
}

struct Aggr {
    int x;
    int y;
};

void foo()
{
    Aggr a;
    a = {};
}

void foo2()
{
    auto a = new Aggr{};
    *a = {};
}
