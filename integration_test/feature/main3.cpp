template <class T>
void __lifetime_contracts(T&&);

int main(int, const char* argv[])
{
    __lifetime_contracts(&main);
    // expected-warning@-1 {{pset(Pre(argv)) = (*argv)}}
    // expected-warning@-2 {{pset(Pre(*argv)) = (**argv)}}
}
