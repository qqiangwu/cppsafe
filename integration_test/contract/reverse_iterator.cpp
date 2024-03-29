struct [[gsl::Pointer(const int)]] Iterator {};

struct [[gsl::Pointer(const int)]] ReverseIterator {
    Iterator iterator;

    ReverseIterator Add(int);
};

template <class T>
void __lifetime_contracts(T&&);

void Foo()
{
    __lifetime_contracts(&ReverseIterator::Add);
    // expected-warning@-1 {{pset(Pre(this)) = (*this)}}
    // expected-warning@-2 {{pset(Pre(*this)) = (**this)}}
    // expected-warning@-3 {{pset(Pre((*this).iterator)) = (*this)}}
    // expected-warning@-4 {{pset(Post((return value))) = (**this)}}
}