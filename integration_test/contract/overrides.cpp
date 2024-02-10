struct Base
{
    virtual void foo1(int* p [[clang::annotate("gsl::lifetime_pre", ":global")]],
                      int* q [[clang::annotate("gsl::lifetime_pre", ":global")]]) = 0;
    virtual void foo2(int** x [[clang::annotate("gsl::lifetime_in")]]) = 0;
};

struct Derived : Base
{
    void foo1(int* p, int* q) override;
    void foo2(int** x) override;
};

struct DeriveRenamed : Base
{
    void foo1(int* p1, int* q1) override;
    void foo2(int** x1) override;
};

template <class T>
void __lifetime_contracts(T&&);

void test()
{
    __lifetime_contracts(&Base::foo1);
    // expected-warning@-1 {{pset(Pre(this)) = (*this)}}
    // expected-warning@-2 {{pset(Pre(p)) = ((global))}}
    // expected-warning@-3 {{pset(Pre(q)) = ((global))}}
    __lifetime_contracts(&Base::foo2);
    // expected-warning@-1 {{pset(Pre(this)) = (*this)}}
    // expected-warning@-2 {{pset(Pre(x)) = ((null), *x)}}
    // expected-warning@-3 {{pset(Pre(*x)) = ((null), **x)}}

    __lifetime_contracts(&Derived::foo1);
    // expected-warning@-1 {{pset(Pre(this)) = (*this)}}
    // expected-warning@-2 {{pset(Pre(p)) = ((global))}}
    // expected-warning@-3 {{pset(Pre(q)) = ((global))}}
    __lifetime_contracts(&Derived::foo2);
    // expected-warning@-1 {{pset(Pre(this)) = (*this)}}
    // expected-warning@-2 {{pset(Pre(x)) = ((null), *x)}}
    // expected-warning@-3 {{pset(Pre(*x)) = ((null), **x)}}

    __lifetime_contracts(&DeriveRenamed::foo1);
    // expected-warning@-1 {{pset(Pre(this)) = (*this)}}
    // expected-warning@-2 {{pset(Pre(p1)) = ((global))}}
    // expected-warning@-3 {{pset(Pre(q1)) = ((global))}}
    __lifetime_contracts(&DeriveRenamed::foo2);
    // expected-warning@-1 {{pset(Pre(this)) = (*this)}}
    // expected-warning@-2 {{pset(Pre(x1)) = ((null), *x1)}}
    // expected-warning@-3 {{pset(Pre(*x1)) = ((null), **x1)}}
}