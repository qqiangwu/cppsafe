// ARGS: --Wlifetime-null
template <class T>
void __lifetime_pset(T&&) {}

template <class T>
void __lifetime_type_category_arg(T&&) {}

struct [[gsl::Owner(int)]] Owner {};

struct Value {};

namespace std {

template <class T>
class function {};

template <class T>
class move_only_function {};

template <class T>
class copyable_function {};

}

int* get();

struct Test {

void test_lambda(int p, double& d, int* q)
{
    __lifetime_pset([]{});  // expected-warning {{pset([]{}) = ((global))}}
    __lifetime_type_category_arg([]{});  // expected-warning {{lifetime type category is Pointer with pointee void}}

    __lifetime_pset([&p, &d, q, this]{});  // expected-warning {{pset([&p, &d, q, this]{}) = ((null), *d, *q, *this, p)}}
    __lifetime_type_category_arg([&p, &d, q, this]{});
    // expected-warning@-1 {{lifetime type category is Pointer with pointee void}}

    int z;
    auto fn = [&z, &p, &d, q, this]{};
    __lifetime_pset(fn);  // expected-warning {{pset(fn) = ((null), *d, *q, *this, p, z)}}
    __lifetime_type_category_arg(fn);
    // expected-warning@-1 {{lifetime type category is Pointer with pointee void}}
}

void test_lambda2()
{
    Owner o;
    Value v;
    auto fn = [&o, &v]{};
    __lifetime_pset(fn);  // expected-warning {{pset(fn) = (o, v)}}
}

void test_lambda_init()
{
    int a;
    int b;
    auto fn = [x = &a, y = get(), &z = b]{};
    __lifetime_pset(fn);  // expected-warning {{((global), a, b)}}
}

void test_functional()
{
    std::function<void()> fn;
    __lifetime_type_category_arg(fn);
    // expected-warning@-1 {{lifetime type category is Pointer with pointee void}}

    __lifetime_type_category_arg(std::move_only_function<void()>{});
    // expected-warning@-1 {{lifetime type category is Pointer with pointee void}}

    __lifetime_type_category_arg(std::copyable_function<void()>{});
    // expected-warning@-1 {{lifetime type category is Pointer with pointee void}}
}

};

void foo3(int* p = nullptr, int* a = 0)
{
    auto m1 = [p]{};
    auto m2 = [a]{};

    __lifetime_pset(m1);  // expected-warning {{pset(m1) = ((null), *p)}}
    __lifetime_pset(m2);  // expected-warning {{pset(m2) = ((null), *a)}}

    __lifetime_pset(p);  // expected-warning {{pset(p) = ((null), *p)}}
    __lifetime_pset(a);  // expected-warning {{pset(a) = ((null), *a)}}

    auto x = p + 1;
    auto m3 = [x]{};
    __lifetime_pset(x);  // expected-warning {{pset(x) = ((null), *p)}}
}