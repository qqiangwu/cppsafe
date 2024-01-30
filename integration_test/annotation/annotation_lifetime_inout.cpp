void Foo(int** p);
void Foo2(int*& p);

void Bar([[clang::annotate("gsl::lifetime_in")]] int** p);
void Bar2([[clang::annotate("gsl::lifetime_in")]] int*& p);

template <class T>
void __lifetime_contracts(T&&);

void test()
{
    __lifetime_contracts(&Foo);
    // expected-warning@-1 {{pset(Pre(p)) = ((null), *p)}}
    // expected-warning@-2 {{pset(Pre(*p)) = ((invalid))}}
    // expected-warning@-3 {{pset(Post(*p)) = ((global))}}

    __lifetime_contracts(&Foo2);
    // expected-warning@-1 {{pset(Pre(p)) = (*p)}}
    // expected-warning@-2 {{pset(Pre(*p)) = ((invalid))}}
    // expected-warning@-3 {{pset(Post(*p)) = ((global))}}

    __lifetime_contracts(&Bar);
    // expected-warning@-1 {{pset(Pre(p)) = ((null), *p)}}
    // expected-warning@-2 {{pset(Pre(*p)) = ((null), **p)}}

    __lifetime_contracts(&Bar2);
    // expected-warning@-1 {{pset(Pre(p)) = (*p)}}
    // expected-warning@-2 {{pset(Pre(*p)) = ((null), **p)}}
}

void Bar3([[clang::annotate("gsl::lifetime_inout")]] int** p)
{
    __lifetime_contracts(&Bar3);
    // expected-warning@-1 {{pset(Pre(p)) = ((null), *p)}}
    // expected-warning@-2 {{pset(Pre(*p)) = ((null), **p)}}
    // expected-warning@-3 {{pset(Post(*p)) = ((null), **p)}}
}