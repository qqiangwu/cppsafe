template <class T>
void __lifetime_pset(T&&);

void __lifetime_pmap();

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

struct Value {
    ~Value();
};

void foo(Value** v)
{
    if (v == nullptr) {
        return;
    }
    *v = new Value{};
}