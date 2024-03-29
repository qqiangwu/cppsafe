struct Cache { };

struct [[gsl::Owner(Cache)]] CachePtr {
    Cache& operator*();
};

template <class T> void __lifetime_pset(T&& t);

template <class T> void __lifetime_contracts(T&& t);

struct CacheHolder {
    CachePtr cache;

    Cache* Get()
    {
        __lifetime_contracts(&CacheHolder::Get);
        // expected-warning@-1 {{pset(Pre(this)) = (*this)}}
        // expected-warning@-2 {{pset(Pre(*this)) = (**this)}}
        // expected-warning@-3 {{pset(Pre((*this).cache)) = (*this)}}
        // expected-warning@-4 {{pset(Post((return value))) = ((null), *this)}}

        __lifetime_pset(cache); // expected-warning {{pset(cache) = (*(*this).cache)}}
        __lifetime_pset(*cache); // expected-warning {{pset(*cache) = (*(*this).cache)}}
        __lifetime_pset(&*cache); // expected-warning {{pset(&*cache) = (*(*this).cache)}}
        return &*cache;
    }
};

void foo()
{
    CacheHolder h;

    auto* p = h.Get();
    // CacheHolder is not a Owner, so **this derived to {h}
    __lifetime_pset(p); // expected-warning {{pset(p) = (h)}}
}
