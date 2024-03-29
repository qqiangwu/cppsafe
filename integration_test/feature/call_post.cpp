template <class T>
void __lifetime_pset(T&&);

void foo(int** p);

struct Test {

int* p;

void test()
{
    delete p;
    __lifetime_pset(p);  // expected-warning {{pset(p) = ((invalid))}}

    foo(&p);
    __lifetime_pset(p);  // expected-warning {{pset(p) = ((global))}}
}

};