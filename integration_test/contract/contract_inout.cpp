template <class T>
void __lifetime_contracts(T&&);

void O1(int*&& p);

void O3(int* const& p);

template <class T>
int O2(T&& p);

int* GetPtr();

void test_output_convention()
{
    __lifetime_contracts(&O1);
    // expected-warning@-1 {{pset(Pre(p)) = (*p)}}
    // expected-warning@-2 {{pset(Pre(*p)) = ((null), **p)}}
    __lifetime_contracts(&O3);
    // expected-warning@-1 {{pset(Pre(p)) = (*p)}}
    // expected-warning@-2 {{pset(Pre(*p)) = ((null), **p)}}

    int* p = nullptr;
    O2(p);
    __lifetime_contracts(O2(p));
    // expected-warning@-1 {{pset(Pre(p)) = (*p)}}
    // expected-warning@-2 {{pset(Pre(*p)) = ((null), **p)}}

    __lifetime_contracts(O2(GetPtr()));
    // expected-warning@-1 {{pset(Pre(p)) = (*p)}}
    // expected-warning@-2 {{pset(Pre(*p)) = ((null), **p)}}
}