#include <memory>

template <class T>
void __lifetime_type_category_arg(T&&);

void test_owners()
{
    std::unique_ptr<char> p1;
    __lifetime_type_category_arg(p1);  // expected-warning {{Owner with pointee char}}

    std::unique_ptr<char[]> p2;
    __lifetime_type_category_arg(p2);  // expected-warning {{Owner with pointee char}}
}