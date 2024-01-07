template <class T>
void __lifetime_pset(T&&) {}

template <class T>
void __lifetime_type_category_arg(T&&) {}

namespace std {

template <class T>
class function {};

template <class T>
class move_only_function {};

template <class T>
class copyable_function {};

}

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