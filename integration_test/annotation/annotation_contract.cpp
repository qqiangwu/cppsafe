template <class T>
void __lifetime_contracts(T&&);

void Foo1(int* p, int* q [[clang::annotate("gsl::lifetime_pre", "p")]]);
void Foo2(int* p, int* q [[clang::annotate("gsl::lifetime_pre", ":global")]]);
void Foo3(int* p, int* q [[clang::annotate("gsl::lifetime_pre", ":invalid")]]);
void Foo4(int* p, int* q [[clang::annotate("gsl::lifetime_pre", ":null")]]);
void Foo5(int* p, int* q [[clang::annotate("gsl::lifetime_pre", "p", ":global")]]);
void Foo6(int* p [[clang::annotate("gsl::lifetime_pre", "this")]]);
    // expected-warning@-1 {{this pre/postcondition is not supported}}

[[clang::annotate("gsl::lifetime_pre", "p", ":global")]]
[[clang::annotate("gsl::lifetime_pre", "*q", ":global")]]
void Foo7(int* p, double** q);

void test_pre()
{
    __lifetime_contracts(&Foo1);
    // expected-warning@-1 {{pset(Pre(p)) = ((null), *p)}}
    // expected-warning@-2 {{pset(Pre(q)) = ((null), *p)}}

    __lifetime_contracts(&Foo2);
    // expected-warning@-1 {{pset(Pre(p)) = ((null), *p)}}
    // expected-warning@-2 {{pset(Pre(q)) = ((global))}}

    __lifetime_contracts(&Foo3);
    // expected-warning@-1 {{pset(Pre(p)) = ((null), *p)}}
    // expected-warning@-2 {{pset(Pre(q)) = ((invalid))}}

    __lifetime_contracts(&Foo4);
    // expected-warning@-1 {{pset(Pre(p)) = ((null), *p)}}
    // expected-warning@-2 {{pset(Pre(q)) = ((null))}}

    __lifetime_contracts(&Foo5);
    // expected-warning@-1 {{pset(Pre(p)) = ((null), *p)}}
    // expected-warning@-2 {{pset(Pre(q)) = ((global), (null), *p)}}

    __lifetime_contracts(&Foo6);
    // expected-warning@-1 {{pset(Pre(p)) = ((null), *p)}}

    __lifetime_contracts(&Foo7);
    // expected-warning@-1 {{pset(Pre(p)) = ((global))}}
    // expected-warning@-2 {{pset(Pre(q)) = ((null), *q)}}
    // expected-warning@-3 {{pset(Pre(*q)) = ((global))}}
    // expected-warning@-4 {{pset(Post(*q)) = ((global))}}
}

struct Out
{
    [[clang::annotate("gsl::lifetime_post", "return", "this")]]
    int* Get();

    [[clang::annotate("gsl::lifetime_post", "return", "*this")]]
    int* GetInner();
    // expected-warning@-2 {{this pre/postcondition is not supported}}

    [[clang::annotate("gsl::lifetime_post", "return", "this")]]
    static int* Get2();
    // expected-warning@-2 {{this pre/postcondition is not supported}}
};

[[clang::annotate("gsl::lifetime_post", "return", "q")]]
int* Bar(int* p, int* q [[clang::annotate("gsl::lifetime_pre", "p")]]);

[[clang::annotate("gsl::lifetime_post", "return", "*p")]]
double* Bar2(int*&& p);

void test_post()
{
    __lifetime_contracts(&Out::Get);
    // expected-warning@-1 {{pset(Pre(this)) = (*this)}}
    // expected-warning@-2 {{pset(Post((return value))) = (*this)}}

    __lifetime_contracts(&Out::GetInner);
    // expected-warning@-1 {{pset(Pre(this)) = (*this)}}
    // expected-warning@-2 {{pset(Post((return value))) = ((global))}}

    __lifetime_contracts(&Out::Get2);
    // expected-warning@-1 {{pset(Post((return value))) = ((global))}}

    __lifetime_contracts(&Bar);
    // expected-warning@-1 {{pset(Pre(p)) = ((null), *p)}}
    // expected-warning@-2 {{pset(Pre(q)) = ((null), *p)}}
    // expected-warning@-3 {{pset(Post((return value))) = ((null), *p)}}

    __lifetime_contracts(&Bar2);
    // expected-warning@-1 {{pset(Pre(p)) = (*p)}}
    // expected-warning@-2 {{pset(Pre(*p)) = ((null), **p)}}
    // expected-warning@-3 {{pset(Post((return value))) = ((null), **p)}}
}
