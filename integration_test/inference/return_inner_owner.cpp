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
        // expected-warning@-3 {{pset(Pre((*this).o)) = (*this)}}
        // expected-warning@-4 {{pset(Post((return value))) = (*this)}}
        return o;
    }

    const Owner& Get1() const
    {
        __lifetime_contracts(&Wrapper::Get1);
        // expected-warning@-1 {{pset(Pre(this)) = (*this)}}
        // expected-warning@-2 {{pset(Pre(*this)) = (**this)}}
        // expected-warning@-3 {{pset(Pre((*this).o)) = (*this)}}
        // expected-warning@-4 {{pset(Post((return value))) = (*this)}}
        return o;
    }

    Owner& Get2()
    {
        __lifetime_contracts(&Wrapper::Get);
        // expected-warning@-1 {{pset(Pre(this)) = (*this)}}
        // expected-warning@-2 {{pset(Pre(*this)) = (**this)}}
        // expected-warning@-3 {{pset(Pre((*this).o)) = (*this)}}
        // expected-warning@-4 {{pset(Post((return value))) = (*this)}}
        return o;
    }

    Owner& Get3() const;

    void test()
    {
        __lifetime_contracts(&Wrapper::Get3);
        // expected-warning@-1 {{pset(Pre(this)) = (*this)}}
        // expected-warning@-2 {{pset(Pre(*this)) = (**this)}}
        // expected-warning@-3 {{pset(Pre((*this).o)) = (*this)}}
        // expected-warning@-4 {{pset(Post((return value))) = ((global))}}
    }
};