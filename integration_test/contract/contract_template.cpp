template <class T>
void __lifetime_contracts(T&&);

template <class T>
void __lifetime_type_category_arg(T&&);

constexpr int Global = 0;

template <class T>
int Foo(T* t [[clang::annotate("gsl::lifetime_pre", Global)]]);

struct Test {
    template <class T> int Foo(T* t [[clang::annotate("gsl::lifetime_pre", Global)]]);
};

template <class T>
struct [[gsl::Pointer]] Ptr
{
    int foo(int* p [[clang::annotate("gsl::lifetime_pre", Global)]]);

    template <class P>
    [[clang::annotate("gsl::lifetime_pre", "p", Global)]]
    int bar(P* p [[clang::annotate("gsl::lifetime_pre", Global)]]);
};

void test_contract()
{
    __lifetime_contracts(&Foo<double>);  // expected-warning {{pset(Pre(t)) = ((global))}}

    __lifetime_contracts(&Test::Foo<double>);
    // expected-warning@-1 {{pset(Pre(this)) = (*this)}}
    // expected-warning@-2 {{pset(Pre(t)) = ((global))}}

    Ptr<int> p;
    __lifetime_type_category_arg(p);
    // expected-warning@-1 {{lifetime type category is Pointer with pointee int}}

    __lifetime_contracts(&Ptr<int>::foo);
    // expected-warning@-1 {{pset(Pre(this)) = (*this)}}
    // expected-warning@-2 {{pset(Pre(*this)) = (**this)}}
    // expected-warning@-3 {{pset(Pre(p)) = ((global))}}

    __lifetime_contracts(&Ptr<int>::bar<int>);
    // expected-warning@-1 {{pset(Pre(this)) = (*this)}}
    // expected-warning@-2 {{pset(Pre(*this)) = (**this)}}
    // expected-warning@-3 {{pset(Pre(p)) = ((global))}}
}