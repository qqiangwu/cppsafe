template <class T>
void __lifetime_contracts(T&&);

template <class T>
struct span
{
    using iterator = T*;
    typedef const T* const_iterator;
};

using A [[clang::annotate("gsl::lifetime_nonnull")]]= int*;
typedef int* B [[clang::annotate("gsl::lifetime_nonnull")]];

void foo(span<int>::iterator it1, span<int>::const_iterator it2)
{
    __lifetime_contracts(&foo);
    // expected-warning@-1 {{(*it1)}}
    // expected-warning@-2 {{(*it2)}}
}

void bar(A a, B b)
{
    __lifetime_contracts(&bar);
    // expected-warning@-1 {{(*a)}}
    // expected-warning@-2 {{(*b)}}
}

struct [[gsl::Pointer(int)]] Ptr1
{
};

struct [[gsl::Pointer(int)]] Ptr2
{
    operator bool();
};

void coo(Ptr1 p1, Ptr2 p2)
{
    __lifetime_contracts(&coo);
    // expected-warning@-1 {{(*p1)}}
    // expected-warning@-2 {{((null), *p2)}}
}