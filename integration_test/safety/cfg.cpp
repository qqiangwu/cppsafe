template <class T>
void __lifetime_pset(T&&) {}

void foo(bool b)
{
    int* p = nullptr;

    for (int i = 0; i < 2; ++i) {
        p = new int{};
    }

    __lifetime_pset(p);  // expected-warning {{((global), (null))}}
}

void foo2(bool b)
{
    int* p = nullptr;

    for (int i = 0; i < 2; ++i) {
        p = new int{};
    }

    if (p) {
        __lifetime_pset(p);  // expected-warning {{((global))}}
    }
}

void bar(bool b)
{
    int* p = nullptr;

    if (b) {
        p = new int{};
    }

    __lifetime_pset(p);  // expected-warning {{((global), (null))}}
}

void bar2(int n)
{
    int* p = nullptr;

    if (n == 1) {
        p = new int{};
    }

    if (n == 2) {
        __lifetime_pset(p);  // expected-warning {{((global), (null))}}
    }
}

void bar3(int n)
{
    int* p = nullptr;

    if (n == 1) {
        p = new int{};
    }

    if (p) {
        __lifetime_pset(p);  // expected-warning {{((global))}}
    }
}

void bar4(int n)
{
    int* p = nullptr;

    if (n == 1) {
        p = new int{};
    } else {
        p = &n;
    }

    __lifetime_pset(p);  // expected-warning {{((global), n)}}
}

bool test_ptr(int* p);

void test_if_nonnull_a1()
{
    int n = 0;
    auto* p = &n;
    __lifetime_pset(p);  // expected-warning {{pset(p) = (n)}}

    if (!p) {  // expected-note {{is compared to null here}}
        __lifetime_pset(p);  // expected-warning {{pset(p) = ((null))}}
        *p;  // expected-warning {{dereferencing a null pointer}}
    } else {
        __lifetime_pset(p);  // expected-warning {{pset(p) = (n)}}
    }
}

void test_if_nonnull_a2()
{
    int n = 0;
    auto* p = &n;
    __lifetime_pset(p);  // expected-warning {{pset(p) = (n)}}

    if (p) {  // expected-note {{is compared to null here}}
        __lifetime_pset(p);  // expected-warning {{pset(p) = (n)}}
    } else {
        __lifetime_pset(p);  // expected-warning {{pset(p) = ((null))}}
        *p;  // expected-warning {{dereferencing a null pointer}}
    }
}

void test_if_nonnull_a3()
{
    int n = 0;
    auto* p = &n;
    if (test_ptr(p)) {
        __lifetime_pset(p);  // expected-warning {{pset(p) = (n)}}
    } else {
        __lifetime_pset(p);  // expected-warning {{pset(p) = (n)}}
    }
}

void test_if_null_b1()
{
    int* p = nullptr;
    if (p) {
        __lifetime_pset(p);  // expected-warning {{pset(p) = ((unknown))}}
    } else {
        __lifetime_pset(p);  // expected-warning {{pset(p) = ((null))}}
    }
}

void test_if_null_b2()
{
    int* p = nullptr;
    if (!p) {
        __lifetime_pset(p);  // expected-warning {{pset(p) = ((null))}}
    } else {
        __lifetime_pset(p);  // expected-warning {{pset(p) = ((unknown))}}
    }
}

void test_if_null_a3()
{
    int* p = nullptr;
    if (test_ptr(p)) {
        __lifetime_pset(p);  // expected-warning {{pset(p) = ((null))}}
    } else {
        __lifetime_pset(p);  // expected-warning {{pset(p) = ((null))}}
    }
}

void test_if_null_iterative()
{
    int x;
    int* p = nullptr;

    while (true) {
        if (p) {
            __lifetime_pset(p);
            // expected-warning@-1 {{pset(p) = ((unknown))}}
            // expected-warning@-2 {{pset(p) = (x)}}
        } else {
            __lifetime_pset(p);  // expected-warning {{pset(p) = ((null))}}
            p = &x;
            __lifetime_pset(p);  // expected-warning {{pset(p) = (x)}}
        }
    }
}

void test_if_nonnull_iterator()
{
    int x;
    int* p = &x;

    while (true) {
        if (p) {
            __lifetime_pset(p);  // expected-warning {{pset(p) = (x)}}
            p = nullptr;
            __lifetime_pset(p);  // expected-warning {{pset(p) = ((null))}}
        } else {
            __lifetime_pset(p);  // expected-warning {{pset(p) = ((null))}}
        }
    }
}
