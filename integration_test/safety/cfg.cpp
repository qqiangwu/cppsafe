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