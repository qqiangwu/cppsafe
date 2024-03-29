template <class T>
void __lifetime_contracts(T&&);

int main(int, char**)
{
    __lifetime_contracts(&main);
    // expected-warning@-1 {{pset(Pre()) = (*)}}
    // expected-warning@-2 {{pset(Pre(*)) = (**)}}
}
