struct [[gsl::Pointer(int)]] Ptr1 {
    Ptr1();

    operator bool();
};

struct [[gsl::Pointer(int)]] Ptr2 {
    Ptr2();
};

template <class T>
void __lifetime_contracts(T&&);

template <class T>
void __lifetime_pset(T&&);

void foo1(int** p)
{
    __lifetime_contracts(&foo1);
    // expected-warning@-1 {{pset(Pre(p)) = ((null), *p)}}
    // expected-warning@-2 {{pset(Pre(*p)) = ((invalid))}}
    // expected-warning@-3 {{pset(Post(*p)) = ((global))}}
}

void foo3(Ptr2* p)
{
    __lifetime_contracts(&foo3);
    // expected-warning@-1 {{pset(Pre(p)) = ((null), *p)}}
    // expected-warning@-2 {{pset(Pre(*p)) = (**p)}}
    // expected-warning@-3 {{pset(Post(*p)) = (**p)}}
}

void test_default_contructor()
{
    Ptr1 p1;
    Ptr2 p2;

    __lifetime_pset(p1);  // expected-warning {{pset(p1) = ((null))}}
    __lifetime_pset(p2);  // expected-warning {{pset(p2) = ((unknown))}}
}

void test_in_out1(Ptr2* input [[clang::annotate("gsl::lifetime_in")]], Ptr2* output);
void test_in_out2(Ptr2& input [[clang::annotate("gsl::lifetime_in")]], Ptr2& output);

void test_in_out()
{
    __lifetime_contracts(test_in_out1);
    // expected-warning@-1 {{pset(Pre(input)) = ((null), *input)}}
    // expected-warning@-2 {{pset(Pre(*input)) = (**input)}}
    // expected-warning@-3 {{pset(Pre(output)) = ((null), *output)}}
    // expected-warning@-4 {{pset(Pre(*output)) = (**output)}}
    // expected-warning@-5 {{pset(Post(*output)) = (**input, **output)}}
    __lifetime_contracts(test_in_out2);
    // expected-warning@-1 {{pset(Pre(input)) = (*input)}}
    // expected-warning@-2 {{pset(Pre(*input)) = (**input)}}
    // expected-warning@-3 {{pset(Pre(output)) = (*output)}}
    // expected-warning@-4 {{pset(Pre(*output)) = (**output)}}
    // expected-warning@-5 {{pset(Post(*output)) = (**input, **output)}}
}