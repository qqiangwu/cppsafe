struct [[gsl::Owner]] A1 {};
struct [[gsl::Owner(int)]] A2 {};

struct [[gsl::Pointer]] B1 {};
struct [[gsl::Pointer(int)]] B2 {};

template <class T>
struct [[gsl::Pointer(T)]] C1 {};

template <class T>
struct [[gsl::Owner(T)]] C2 {};

template <class T>
void __lifetime_type_category();

void test_annotation()
{
    __lifetime_type_category<A1>();  // expected-warning {{lifetime type category is Owner with pointee void}}
    __lifetime_type_category<A2>();  // expected-warning {{lifetime type category is Owner with pointee int}}

    __lifetime_type_category<B1>();  // expected-warning {{lifetime type category is Pointer with pointee void}}
    __lifetime_type_category<B2>();  // expected-warning {{lifetime type category is Pointer with pointee int}}

    __lifetime_type_category<C1<int>>();  // expected-warning {{lifetime type category is Pointer with pointee int}}
    __lifetime_type_category<C2<int>>();  // expected-warning {{lifetime type category is Owner with pointee int}}
}