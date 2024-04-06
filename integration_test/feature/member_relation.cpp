// ARGS: --Wlifetime-move
struct Value {
    int x;
    int y;

    Value(Value&&);

    virtual ~Value();

    void foo();
};

struct [[gsl::Owner(Value)]] Owner {
    Value& get();
};

namespace std {

template <class T>
__remove_reference_t(T)&& move(T&& t);

}

template <class T>
void use(T&&);

template <class T> void __lifetime_pset(T&&);

void __lifetime_pmap();

struct Type {
    Value a;
    Value b;

    void foo()
    {
        __lifetime_pset(a);  // expected-warning {{((*this).a)}}
        __lifetime_pset(b);  // expected-warning {{((*this).b)}}

        use(std::move(a));  // expected-note 2 {{moved here}}

        __lifetime_pset(a);  // expected-warning {{((invalid))}}
        __lifetime_pset(b);  // expected-warning {{((*this).b)}}

        a.foo();  // expected-warning {{use a moved-from object}}
        b.foo();
    }  // expected-warning {{returning a dangling pointer as output value '(*this).a'}}
};

#if 0
struct Type2 {
    Owner a;
    Owner b;

    void foo()
    {
        __lifetime_pset(a);  // expected-warning {{(*(*this).a)}}

        auto& f = a.get();
        __lifetime_pset(f);  // expected-warning {{pset(f) = (*(*this).a)}}

        auto x = std::move(f);
        // expected-note@-1 {{moved here}}

        __lifetime_pset(a);  // expected-warning {{pset(a) = ((invalid))}}
        __lifetime_pset(b);  // expected-warning {{(*(*this).b)}}

        f.foo();  // expected-warning {{dereferencing a dangling pointer}}
    }
};
#endif