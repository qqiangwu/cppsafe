struct [[gsl::Owner(int)]] Owner {};

template <class T>
void __lifetime_contracts(T&&);

struct Wrapper
{
    Owner o;

    const Owner& Get()
    {
        __lifetime_contracts(&Wrapper::Get);
        // expected-warning@-1 {{pset(Pre(this)) = (*this)}}
        // expected-warning@-2 {{pset(Pre(*this)) = (**this)}}
        // expected-warning@-3 {{pset(Pre((*this).o)) = (**this)}}
        // expected-warning@-4 {{pset(Post((return value))) = (**this)}}
        return o;
    }
};