// ARGS: --Wlifetime-move
struct [[gsl::Owner(int)]] Owner {};

struct Value {
    virtual ~Value() {}
};

struct Aggr
{
    Owner o1;
    Owner o2;
    Value v1;
    Value v2;
};

template <class T>
void __lifetime_pset(T&&) {}

void __lifetime_pmap();

namespace std {

template <class T>
__remove_reference_t(T)&& move(T&& t);

}

template <class T>
void eat(T&&) {}

void test_local()
{
    Aggr a;

    __lifetime_pset(a);  // expected-warning {{pset(a) = (a)}}
    __lifetime_pset(a.o1);  // expected-warning {{pset(a.o1) = (*a.o1)}}
    __lifetime_pset(a.o2);  // expected-warning {{pset(a.o2) = (*a.o2)}}
    __lifetime_pset(a.v1);  // expected-warning {{pset(a.v1) = (a.v1)}}
    __lifetime_pset(a.v2);  // expected-warning {{pset(a.v2) = (a.v2)}}

    eat(std::move(a.o1));
    __lifetime_pset(a);  // expected-warning {{pset(a) = (a)}}
    __lifetime_pset(a.o1);  // expected-warning {{pset(a.o1) = ((invalid))}}
    __lifetime_pset(a.o2);  // expected-warning {{pset(a.o2) = (*a.o2)}}
    __lifetime_pset(a.v1);  // expected-warning {{pset(a.v1) = (a.v1)}}
    __lifetime_pset(a.v2);  // expected-warning {{pset(a.v2) = (a.v2)}}

    eat(std::move(a.v1));
    __lifetime_pset(a);  // expected-warning {{pset(a) = (a)}}
    __lifetime_pset(a.o1);  // expected-warning {{pset(a.o1) = ((invalid))}}
    __lifetime_pset(a.o2);  // expected-warning {{pset(a.o2) = (*a.o2)}}
    __lifetime_pset(a.v1);  // expected-warning {{pset(a.v1) = ((invalid))}}
    __lifetime_pset(a.v2);  // expected-warning {{pset(a.v2) = (a.v2)}}

    eat(std::move(a));
    __lifetime_pset(a);  // expected-warning {{pset(a) = ((invalid))}}
    __lifetime_pset(a.o1);  // expected-warning {{pset(a.o1) = ((invalid))}}
    __lifetime_pset(a.o2);  // expected-warning {{pset(a.o2) = ((invalid))}}
    __lifetime_pset(a.v1);  // expected-warning {{pset(a.v1) = ((invalid))}}
    __lifetime_pset(a.v2);  // expected-warning {{pset(a.v2) = ((invalid))}}
}

void test_local2()
{
    Aggr a;

    eat(std::move(a.o1));
    __lifetime_pset(a);  // expected-warning {{pset(a) = (a)}}
    __lifetime_pset(a.o1);  // expected-warning {{pset(a.o1) = ((invalid))}}
    __lifetime_pset(a.o2);  // expected-warning {{pset(a.o2) = (*a.o2)}}
    __lifetime_pset(a.v1);  // expected-warning {{pset(a.v1) = (a.v1)}}
    __lifetime_pset(a.v2);  // expected-warning {{pset(a.v2) = (a.v2)}}
}

void test_param(Aggr a)
{
    __lifetime_pset(a);  // expected-warning {{pset(a) = (a)}}
    __lifetime_pset(a.o1);  // expected-warning {{pset(a.o1) = (*a.o1)}}
    __lifetime_pset(a.o2);  // expected-warning {{pset(a.o2) = (*a.o2)}}
    __lifetime_pset(a.v1);  // expected-warning {{pset(a.v1) = (a.v1)}}
    __lifetime_pset(a.v2);  // expected-warning {{pset(a.v2) = (a.v2)}}

    eat(std::move(a.o1));
    __lifetime_pset(a);  // expected-warning {{pset(a) = (a)}}
    __lifetime_pset(a.o1);  // expected-warning {{pset(a.o1) = ((invalid))}}
    __lifetime_pset(a.o2);  // expected-warning {{pset(a.o2) = (*a.o2)}}
    __lifetime_pset(a.v1);  // expected-warning {{pset(a.v1) = (a.v1)}}
    __lifetime_pset(a.v2);  // expected-warning {{pset(a.v2) = (a.v2)}}

    eat(std::move(a.v1));
    __lifetime_pset(a);  // expected-warning {{pset(a) = (a)}}
    __lifetime_pset(a.o1);  // expected-warning {{pset(a.o1) = ((invalid))}}
    __lifetime_pset(a.o2);  // expected-warning {{pset(a.o2) = (*a.o2)}}
    __lifetime_pset(a.v1);  // expected-warning {{pset(a.v1) = ((invalid))}}
    __lifetime_pset(a.v2);  // expected-warning {{pset(a.v2) = (a.v2)}}

    eat(std::move(a));
    __lifetime_pset(a);  // expected-warning {{pset(a) = ((invalid))}}
    __lifetime_pset(a.o1);  // expected-warning {{pset(a.o1) = ((invalid))}}
    __lifetime_pset(a.o2);  // expected-warning {{pset(a.o2) = ((invalid))}}
    __lifetime_pset(a.v1);  // expected-warning {{pset(a.v1) = ((invalid))}}
    __lifetime_pset(a.v2);  // expected-warning {{pset(a.v2) = ((invalid))}}
}

Aggr foo();

void test_return()
{
    auto a = foo();

    __lifetime_pset(a);  // expected-warning {{pset(a) = (a)}}
    __lifetime_pset(a.o1);  // expected-warning {{pset(a.o1) = (*a.o1)}}
    __lifetime_pset(a.o2);  // expected-warning {{pset(a.o2) = (*a.o2)}}
    __lifetime_pset(a.v1);  // expected-warning {{pset(a.v1) = (a.v1)}}
    __lifetime_pset(a.v2);  // expected-warning {{pset(a.v2) = (a.v2)}}

    eat(std::move(a.o1));
    __lifetime_pset(a);  // expected-warning {{pset(a) = (a)}}
    __lifetime_pset(a.o1);  // expected-warning {{pset(a.o1) = ((invalid))}}
    __lifetime_pset(a.o2);  // expected-warning {{pset(a.o2) = (*a.o2)}}
    __lifetime_pset(a.v1);  // expected-warning {{pset(a.v1) = (a.v1)}}
    __lifetime_pset(a.v2);  // expected-warning {{pset(a.v2) = (a.v2)}}

    eat(std::move(a.v1));
    __lifetime_pset(a);  // expected-warning {{pset(a) = (a)}}
    __lifetime_pset(a.o1);  // expected-warning {{pset(a.o1) = ((invalid))}}
    __lifetime_pset(a.o2);  // expected-warning {{pset(a.o2) = (*a.o2)}}
    __lifetime_pset(a.v1);  // expected-warning {{pset(a.v1) = ((invalid))}}
    __lifetime_pset(a.v2);  // expected-warning {{pset(a.v2) = (a.v2)}}

    eat(std::move(a));
    __lifetime_pset(a);  // expected-warning {{pset(a) = ((invalid))}}
    __lifetime_pset(a.o1);  // expected-warning {{pset(a.o1) = ((invalid))}}
    __lifetime_pset(a.o2);  // expected-warning {{pset(a.o2) = ((invalid))}}
    __lifetime_pset(a.v1);  // expected-warning {{pset(a.v1) = ((invalid))}}
    __lifetime_pset(a.v2);  // expected-warning {{pset(a.v2) = ((invalid))}}
}

void test_assignment()
{
    Aggr a;
    eat(std::move(a));
    __lifetime_pset(a);  // expected-warning {{pset(a) = ((invalid))}}
    __lifetime_pset(a.o1);  // expected-warning {{pset(a.o1) = ((invalid))}}
    __lifetime_pset(a.o2);  // expected-warning {{pset(a.o2) = ((invalid))}}
    __lifetime_pset(a.v1);  // expected-warning {{pset(a.v1) = ((invalid))}}
    __lifetime_pset(a.v2);  // expected-warning {{pset(a.v2) = ((invalid))}}

    a = {};
    __lifetime_pset(a);  // expected-warning {{pset(a) = (a)}}
    __lifetime_pset(a.o1);  // expected-warning {{pset(a.o1) = (*a.o1)}}
    __lifetime_pset(a.o2);  // expected-warning {{pset(a.o2) = (*a.o2)}}
    __lifetime_pset(a.v1);  // expected-warning {{pset(a.v1) = (a.v1)}}
    __lifetime_pset(a.v2);  // expected-warning {{pset(a.v2) = (a.v2)}}
}

void test_compound_assignment()
{
    Aggr a;
    eat(std::move(a));
    __lifetime_pset(a);  // expected-warning {{pset(a) = ((invalid))}}
    __lifetime_pset(a.o1);  // expected-warning {{pset(a.o1) = ((invalid))}}
    __lifetime_pset(a.o2);  // expected-warning {{pset(a.o2) = ((invalid))}}
    __lifetime_pset(a.v1);  // expected-warning {{pset(a.v1) = ((invalid))}}
    __lifetime_pset(a.v2);  // expected-warning {{pset(a.v2) = ((invalid))}}

    a = {
        .o1 = {},
        .v1 = {},
    };
    __lifetime_pset(a);  // expected-warning {{pset(a) = (a)}}
    __lifetime_pset(a.o1);  // expected-warning {{pset(a.o1) = (*a.o1)}}
    __lifetime_pset(a.o2);  // expected-warning {{pset(a.o2) = (*a.o2)}}
    __lifetime_pset(a.v1);  // expected-warning {{pset(a.v1) = (a.v1)}}
    __lifetime_pset(a.v2);  // expected-warning {{pset(a.v2) = (a.v2)}}
}