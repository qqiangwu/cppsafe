// ARGS: --Wlifetime-null

int* get();
int* get(int*);

[[clang::annotate("gsl::lifetime_post", "return", ":global", ":null")]]
int* get2();

void access(int m);

template <class T>
void __lifetime_pset(T&&);

void test1(bool b)
{
    int m = 0;
    int* p = &m;
    if (b) {
        p = nullptr;  // expected-note {{assigned here}}
    }

    access(*p);  // expected-warning {{dereferencing a possibly null pointer}}
}

void test2(bool b)
{
    int m = 0;
    int* p = &m;
    if (b) {
        p = get();  // expected-note {{assigned here}}
        __lifetime_pset(p);  // expected-warning {{pset(p) = ((global), (null))}}
    }

    access(*p);  // expected-warning {{dereferencing a possibly null pointer}}
}

void test3(bool b)
{
    int n = 0;
    int m = 0;
    int* p = &m;
    if (b) {
        p = get(&n);
        __lifetime_pset(p);  // expected-warning {{(n)}}
    }
}

void out(int* p, int** q);

void test4(bool b)
{
    int m = 0;
    int* p = &m;
    if (b) {
        out(get(), &p);
        __lifetime_pset(p);  // expected-warning {{pset(p) = ((global), (null))}}
    }
}

void test5(bool b)
{
    int n = 0;
    int m = 0;
    int* p = &m;
    if (b) {
        out(&n, &p);
        __lifetime_pset(p);  // expected-warning {{(n)}}
    }
}

void test6(bool b)
{
    int m = 0;
    int* p = &m;
    if (b) {
        out(nullptr, &p);
        __lifetime_pset(p);  // expected-warning {{((null))}}
    }
}

void test7(bool b)
{
    int* p = get2();
    __lifetime_pset(p);  // expected-warning {{((global), (null))}}
}