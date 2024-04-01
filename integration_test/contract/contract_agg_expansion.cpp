template <class T>
void __lifetime_contracts(T&&);

template <class T>
void __lifetime_pset(T&&);

struct Agg
{
    int i;
    double* p;
};

double* f1(Agg a);
double* f2(Agg* a);
double* f3(Agg& a);
double* f4(const Agg& a);
int* f5(Agg& a);
int* f6(const Agg& a);

struct Nested
{
    int i;
    int* p;
    Agg a1;
    Agg* a2;

    double* f7();
};

double* g1(Nested a);
double* g2(Nested* a);
double* g3(Nested& a);
double* g4(const Nested& a);

#if 0
void test_parameter_expansion_by_value()
{
    __lifetime_contracts(&f1);
    // expected-warning@-1 {{pset(Pre(a.p)) = ((null), *a.p)}}
    // expected-warning@-2 {{pset(Post((return value))) = ((null), *a.p)}}

    double d = 0;
    Agg agg;
    agg.p = &d;
    __lifetime_pset(agg.p);
    auto* p = f1(agg);
    __lifetime_pset(p);
}

void test_parameter_expansion_by_value_nested()
{
    __lifetime_contracts(&g1);
    // expected-warning@-1 {{pset(Pre(a.p)) = ((null), *a.p)}}
    // expected-warning@-2 {{pset(Pre(a.a2)) = ((null), *a.a2)}}
    // expected-warning@-3 {{pset(Pre(a.a1.p)) = ((null), *a.a1.p)}}
    // expected-warning@-4 {{pset(Post((return value))) = ((null), *a.a1.p)}}
}
#endif

void test_parameter_expansion_by_pointer()
{
    __lifetime_contracts(&f2);
    // expected-warning@-1 {{pset(Pre(a)) = ((null), *a)}}
    // expected-warning@-2 {{pset(Pre((*a).i)) = ((null), *a)}}
    // expected-warning@-3 {{pset(Post((return value))) = ((global), (null))}}

    double d;
    Agg agg{ .p = &d};
    auto* p1 = f2(&agg);
    __lifetime_pset(p1);  // expected-warning {{pset(p1) = ((global))}}

    __lifetime_contracts(&f3);
    // expected-warning@-1 {{pset(Pre(a)) = (*a)}}
    // expected-warning@-2 {{pset(Pre((*a).i)) = (*a)}}
    // expected-warning@-3 {{pset(Post((return value))) = ((global), (null))}}
    auto* p2 = f3(agg);
    __lifetime_pset(p2);  // expected-warning {{pset(p2) = ((global))}}

    __lifetime_contracts(&f4);
    // expected-warning@-1 {{pset(Pre(a)) = (*a)}}
    // expected-warning@-2 {{pset(Pre((*a).i)) = (*a)}}
    // expected-warning@-3 {{pset(Post((return value))) = ((global), (null))}}

    auto* p3 = f4(agg);
    __lifetime_pset(p3);  // expected-warning {{pset(p3) = ((global))}}

    __lifetime_contracts(&f5);
    // expected-warning@-1 {{pset(Pre(a)) = (*a)}}
    // expected-warning@-2 {{pset(Pre((*a).i)) = (*a)}}
    // expected-warning@-3 {{pset(Post((return value))) = ((null), *a)}}

    auto* p4 = f5(agg);
    __lifetime_pset(p4);  // expected-warning {{pset(p4) = (agg)}}

    __lifetime_contracts(&f6);
    // expected-warning@-1 {{pset(Pre(a)) = (*a)}}
    // expected-warning@-2 {{pset(Pre((*a).i)) = (*a)}}
    // expected-warning@-3 {{pset(Post((return value))) = ((global), (null))}}

    auto* p5 = f6(agg);
    __lifetime_pset(p5);  // expected-warning {{pset(p5) = ((global))}}
}

void test_parameter_expansion_by_pointer_nested()
{
    __lifetime_contracts(&g2);
    // expected-warning@-1 {{pset(Pre(a)) = ((null), *a)}}
    // expected-warning@-2 {{pset(Pre((*a).i)) = ((null), *a)}}
    // expected-warning@-3 {{pset(Pre((*a).a1)) = ((null), *a)}}
    // expected-warning@-4 {{pset(Pre((*a).a1.i)) = ((null), *a)}}
    // expected-warning@-5 {{pset(Post((return value))) = ((global), (null))}}

    __lifetime_contracts(&g3);
    // expected-warning@-1 {{pset(Pre(a)) = (*a)}}
    // expected-warning@-2 {{pset(Pre((*a).i)) = (*a)}}
    // expected-warning@-3 {{pset(Pre((*a).a1)) = (*a)}}
    // expected-warning@-4 {{pset(Pre((*a).a1.i)) = (*a)}}
    // expected-warning@-5 {{pset(Post((return value))) = ((global), (null))}}

    __lifetime_contracts(&g4);
    // expected-warning@-1 {{pset(Pre(a)) = (*a)}}
    // expected-warning@-2 {{pset(Pre((*a).i)) = (*a)}}
    // expected-warning@-3 {{pset(Pre((*a).a1)) = (*a)}}
    // expected-warning@-4 {{pset(Pre((*a).a1.i)) = (*a)}}
    // expected-warning@-5 {{pset(Post((return value))) = ((global), (null))}}
}

void test_parameter_expansion_this()
{
    __lifetime_contracts(&Nested::f7);
    // expected-warning@-1 {{pset(Pre(this)) = (*this)}}
    // expected-warning@-2 {{pset(Pre((*this).i)) = (*this)}}
    // expected-warning@-3 {{pset(Pre((*this).a1)) = (*this)}}
    // expected-warning@-4 {{pset(Pre((*this).a1.i)) = (*this)}}
    // expected-warning@-5 {{pset(Post((return value))) = ((global), (null))}}
}

// https://godbolt.org/z/wncC9a
struct X { int a, b; };

int& example_2_6_2_4(X& x)
{
    __lifetime_contracts(&example_2_6_2_4);
    // expected-warning@-1 {{pset(Pre(x)) = (*x)}}
    // expected-warning@-2 {{pset(Pre((*x).a)) = (*x)}}
    // expected-warning@-3 {{pset(Pre((*x).b)) = (*x)}}
    // expected-warning@-4 {{pset(Post((return value))) = (*x)}}

    return x.b;
}

void foo(X& x, int*& p)
{
    p = &x.a;
}

struct [[gsl::Owner(int)]] IntOwner {};

struct [[gsl::Pointer(const int)]] IntPtr {
    IntPtr(const IntOwner&);
};

struct OwnerAgg
{
    IntOwner o;
};

IntOwner& get1(OwnerAgg* agg);

IntOwner& get2(OwnerAgg& agg);

IntOwner& get3(const OwnerAgg* agg);

const IntOwner& get4(OwnerAgg* agg);

const IntOwner& get5(const OwnerAgg* agg);

struct OwnerThis
{
    IntOwner o;

    IntOwner& get1();
    IntOwner& get2() const;

    const IntOwner& get3();
    const IntOwner& get4() const;
};

struct ValueAggr {
    virtual ~ValueAggr();

    IntOwner o;
};

IntPtr getConstOwner(const ValueAggr* v)
{
    return v->o;
}

void test_member_owner()
{
    __lifetime_contracts(&get1);
    // expected-warning@-1 {{pset(Pre(agg)) = ((null), *agg)}}
    // expected-warning@-2 {{pset(Pre(*agg)) = (**agg)}}
    // expected-warning@-3 {{pset(Pre((*agg).o)) = ((null), *agg)}}
    // expected-warning@-4 {{pset(Post((return value))) = (*agg)}}
    __lifetime_contracts(&get2);
    // expected-warning@-1 {{pset(Pre(agg)) = (*agg)}}
    // expected-warning@-2 {{pset(Pre(*agg)) = (**agg)}}
    // expected-warning@-3 {{pset(Pre((*agg).o)) = (*agg)}}
    // expected-warning@-4 {{pset(Post((return value))) = (*agg)}}

    __lifetime_contracts(&get3);
    // expected-warning@-1 {{pset(Pre(agg)) = ((null), *agg)}}
    // expected-warning@-2 {{pset(Pre(*agg)) = (**agg)}}
    // expected-warning@-3 {{pset(Pre((*agg).o)) = ((null), *agg)}}
    // expected-warning@-4 {{pset(Post((return value))) = ((global))}}

    __lifetime_contracts(&get4);
    // expected-warning@-1 {{pset(Pre(agg)) = ((null), *agg)}}
    // expected-warning@-2 {{pset(Pre(*agg)) = (**agg)}}
    // expected-warning@-3 {{pset(Pre((*agg).o)) = ((null), *agg)}}
    // expected-warning@-4 {{pset(Post((return value))) = (*agg)}}

    __lifetime_contracts(&get5);
    // expected-warning@-1 {{pset(Pre(agg)) = ((null), *agg)}}
    // expected-warning@-2 {{pset(Pre(*agg)) = (**agg)}}
    // expected-warning@-3 {{pset(Pre((*agg).o)) = ((null), *agg)}}
    // expected-warning@-4 {{pset(Post((return value))) = (*agg)}}

    __lifetime_contracts(&OwnerThis::get1);
    // expected-warning@-1 {{pset(Pre(this)) = (*this)}}
    // expected-warning@-2 {{pset(Pre(*this)) = (**this)}}
    // expected-warning@-3 {{pset(Pre((*this).o)) = (*this)}}
    // expected-warning@-4 {{pset(Post((return value))) = (*this)}}
    __lifetime_contracts(&OwnerThis::get2);
    // expected-warning@-1 {{pset(Pre(this)) = (*this)}}
    // expected-warning@-2 {{pset(Pre(*this)) = (**this)}}
    // expected-warning@-3 {{pset(Pre((*this).o)) = (*this)}}
    // expected-warning@-4 {{pset(Post((return value))) = ((global))}}
    __lifetime_contracts(&OwnerThis::get3);
    // expected-warning@-1 {{pset(Pre(this)) = (*this)}}
    // expected-warning@-2 {{pset(Pre(*this)) = (**this)}}
    // expected-warning@-3 {{pset(Pre((*this).o)) = (*this)}}
    // expected-warning@-4 {{pset(Post((return value))) = (*this)}}
    __lifetime_contracts(&OwnerThis::get4);
    // expected-warning@-1 {{pset(Pre(this)) = (*this)}}
    // expected-warning@-2 {{pset(Pre(*this)) = (**this)}}
    // expected-warning@-3 {{pset(Pre((*this).o)) = (*this)}}
    // expected-warning@-4 {{pset(Post((return value))) = (*this)}}

    __lifetime_contracts(&getConstOwner);
    // expected-warning@-1 {{pset(Pre(v)) = ((null), *v)}}
    // expected-warning@-2 {{pset(Pre(*v)) = (**v)}}
    // expected-warning@-3 {{pset(Pre((*v).o)) = ((null), *v)}}
    // expected-warning@-4 {{pset(Post((return value))) = (*v)}}
}