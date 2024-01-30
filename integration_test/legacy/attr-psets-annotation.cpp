template <typename T>
bool __lifetime_pset(const T &) { return true; }

template <typename T>
bool __lifetime_pset_ref(const T &) { return true; }

template <typename T>
void __lifetime_type_category() {}

template <typename T>
bool __lifetime_contracts(const T &) { return true; }

constexpr int Global = 0;
constexpr int Invalid = 0;
constexpr int Null = 0;
constexpr int Return = 0;

struct [[gsl::Pointer]] my_pointer {
  my_pointer();
  my_pointer &operator=(const my_pointer &);
  int &operator*();
};

[[clang::annotate("gsl::lifetime_pre", "b", "a")]]
void basic(int *a, int *b) {
  __lifetime_pset(b); // expected-warning {{((null), *a)}}
}

// Proper lifetime preconditions can only be checked with annotations.
void check_lifetime_preconditions() {
  int a, b;
  basic(&a, &a);
  basic(&b, &b);
  basic(&a,
        &b); // expected-warning {{passing a pointer as argument with points-to set (b) where points-to set ((null), a) is expected}}
}

[[clang::annotate("gsl::lifetime_pre", "a", Null)]]
[[clang::annotate("gsl::lifetime_pre", "b", Global)]]
[[clang::annotate("gsl::lifetime_pre", "c", Invalid)]]
void specials(int *a, int *b, int *c) {
  __lifetime_pset(a); // expected-warning {{((null))}}
  __lifetime_pset(b); // expected-warning {{((global))}}
  __lifetime_pset(c); // expected-warning {{((invalid))}}
}

[[clang::annotate("gsl::lifetime_pre", "b", "a", "c")]]
void variadic(int *a, int *b, int *c) {
  __lifetime_pset(b); // expected-warning {{((null), *a, *c}}
}

[[clang::annotate("gsl::lifetime_pre", "b", "a", Null)]]
void variadic_special(int *a, int *b, int *c) {
  __lifetime_pset(b); // expected-warning {{((null), *a}}
}

[[clang::annotate("gsl::lifetime_pre", "b", "a")]]
[[clang::annotate("gsl::lifetime_pre", "c", "a")]]
void multiple_annotations(int *a, int *b, int *c) {
  __lifetime_pset(b); // expected-warning {{((null), *a)}}
  __lifetime_pset(c); // expected-warning {{((null), *a)}}
}

[[clang::annotate("gsl::lifetime_pre", "b", "a")]]
[[clang::annotate("gsl::lifetime_pre", "c", "b")]]
void multiple_annotations_chained(int *a, int *b, int *c) {
  __lifetime_pset(b); // expected-warning {{((null), *a)}}
  __lifetime_pset(c); // expected-warning {{((null), *a)}}
}

[[clang::annotate("gsl::lifetime_pre", "b", "a")]]
void annotate_forward_decl(int *a, int *b);

void annotate_forward_decl(int *c, int *d) {
  __lifetime_pset(d); // expected-warning {{((null), *c)}}
}

// Repeated annotations on redeclarations are not checked as
// they will automatically be checked with contracts.

namespace std {

template <typename T>
struct unique_ptr {
  T &operator*() const;
  explicit operator bool() const;
  ~unique_ptr();
};

}

namespace dump_contracts {
// Need to have bodies to fill the lifetime attr.
void p(int *a) {}
void p2(int *a, int &b) {}
void p3(int *a, int *&b) { b = 0; }
void parameter_psets(int value,
                     char *const *in,
                     int &int_ref,
                     const int &const_int_ref,
                     std::unique_ptr<int> owner_by_value,
                     const std::unique_ptr<int> &owner_const_ref,
                     std::unique_ptr<int> &owner_ref,
                     my_pointer ptr_by_value,
                     const my_pointer &ptr_const_ref,
                     my_pointer &ptr_ref,
                     my_pointer *ptr_ptr,
                     const my_pointer *ptr_const_ptr) {}

[[clang::annotate("gsl::lifetime_pre", "b", "a")]]
void p4(int *a, int *b, int *&c) { c = 0; }
int *p5(int *a, int *b) { return a; }

[[clang::annotate("gsl::lifetime_post", Return, "a")]]
int *p6(int *a, int *b) { return a; }

struct S{
  int *f(int * a, int *b, int *&c) { c = 0; return a; }
  S *g(int * a, int *b, int *&c) { c = 0; return this; }
};

[[clang::annotate("gsl::lifetime_post", "*c", "a")]]
void p7(int *a, int *b, int *&c) { c = a; }
[[clang::annotate("gsl::lifetime_post", "*c", "a")]]
void p8(int *a, int *b, int **c) { if (c) *c = a; }
void p9(int *a, int *b, int *&c [[clang::annotate("gsl::lifetime_in")]]) {}
int* p10() { return 0; }  // expected-warning {{returning a null pointer where a non-null pointer is expected}}
// TODO: contracts for function pointers?

void f() {
  __lifetime_contracts(p);
  // expected-warning@-1 {{pset(Pre(a)) = ((null), *a)}}
  __lifetime_contracts(p2);
  // expected-warning@-1 {{pset(Pre(a)) = ((null), *a)}}
  // expected-warning@-2 {{pset(Pre(b)) = (*b)}}
  __lifetime_contracts(p3);
  // expected-warning@-1 {{pset(Pre(a)) = ((null), *a)}}
  // expected-warning@-2 {{pset(Pre(b)) = (*b)}}
  // expected-warning@-3 {{pset(Pre(*b)) = ((invalid))}}
  // expected-warning@-4 {{pset(Post(*b)) = ((null), *a)}}
  __lifetime_contracts(parameter_psets);
  // expected-warning@-1 {{pset(Pre(owner_by_value)) = (*owner_by_value)}}
  // expected-warning@-2 {{pset(Pre(owner_ref)) = (*owner_ref)}}
  // expected-warning@-3 {{pset(Pre(*owner_ref)) = (**owner_ref)}}
  // expected-warning@-4 {{pset(Pre(ptr_ref)) = (*ptr_ref)}}
  // expected-warning@-5 {{pset(Pre(*ptr_ref)) = (**ptr_ref)}}
  // expected-warning@-6 {{pset(Pre(ptr_const_ref)) = (*ptr_const_ref)}}
  // expected-warning@-7 {{pset(Pre(*ptr_const_ref)) = (**ptr_const_ref)}}
  // expected-warning@-8 {{pset(Pre(ptr_const_ptr)) = ((null), *ptr_const_ptr)}}
  // expected-warning@-9 {{pset(Pre(*ptr_const_ptr)) = (**ptr_const_ptr)}}
  // expected-warning@-10 {{pset(Pre(in)) = ((null), *in)}}
  // expected-warning@-11 {{pset(Pre(*in)) = ((null), **in)}}
  // expected-warning@-12 {{pset(Pre(owner_const_ref)) = (*owner_const_ref)}}
  // expected-warning@-13 {{pset(Pre(*owner_const_ref)) = (**owner_const_ref)}}
  // expected-warning@-14 {{pset(Pre(int_ref)) = (*int_ref)}}
  // expected-warning@-15 {{pset(Pre(const_int_ref)) = (*const_int_ref)}}
  // expected-warning@-16 {{pset(Pre(ptr_ptr)) = ((null), *ptr_ptr)}}
  // expected-warning@-17 {{pset(Pre(*ptr_ptr)) = (**ptr_ptr)}}
  // expected-warning@-18 {{pset(Pre(ptr_by_value)) = (*ptr_by_value)}}
  // expected-warning@-19 {{pset(Post(*ptr_ref)) = (**owner_ref, **ptr_const_ptr, **ptr_const_ref, **ptr_ptr, **ptr_ref, *int_ref, *ptr_by_value)}}
  // expected-warning@-20 {{pset(Post(*ptr_ptr)) = (**owner_ref, **ptr_const_ptr, **ptr_const_ref, **ptr_ptr, **ptr_ref, *int_ref, *ptr_by_value)}}
  __lifetime_contracts(p4);
  // expected-warning@-1 {{pset(Pre(a)) = ((null), *a)}}
  // expected-warning@-2 {{pset(Pre(b)) = ((null), *a)}}
  // expected-warning@-3 {{pset(Pre(c)) = (*c)}}
  // expected-warning@-4 {{pset(Pre(*c)) = ((invalid))}}
  // expected-warning@-5 {{pset(Post(*c)) = ((null), *a)}}
  __lifetime_contracts(p5);
  // expected-warning@-1 {{pset(Pre(a)) = ((null), *a)}}
  // expected-warning@-2 {{pset(Pre(b)) = ((null), *b)}}
  // expected-warning@-3 {{pset(Post((return value))) = ((null), *a, *b)}}
  __lifetime_contracts(p6);
  // expected-warning@-1 {{pset(Pre(a)) = ((null), *a)}}
  // expected-warning@-2 {{pset(Pre(b)) = ((null), *b)}}
  // expected-warning@-3 {{pset(Post((return value))) = ((null), *a)}}
  __lifetime_contracts(&S::f);
  // expected-warning@-1 {{pset(Pre(a)) = ((null), *a)}}
  // expected-warning@-2 {{pset(Pre(b)) = ((null), *b)}}
  // expected-warning@-3 {{pset(Pre(c)) = (*c)}}
  // expected-warning@-4 {{pset(Pre(*c)) = ((invalid))}}
  // expected-warning@-5 {{pset(Pre(this)) = (*this)}}
  // expected-warning@-6 {{pset(Post(*c)) = ((null), *a, *b)}}
  // expected-warning@-7 {{pset(Post((return value))) = ((null), *a, *b)}}
  __lifetime_contracts(&S::g);
  // expected-warning@-1 {{pset(Pre(a)) = ((null), *a)}}
  // expected-warning@-2 {{pset(Pre(b)) = ((null), *b)}}
  // expected-warning@-3 {{pset(Pre(c)) = (*c)}}
  // expected-warning@-4 {{pset(Pre(*c)) = ((invalid))}}
  // expected-warning@-5 {{pset(Pre(this)) = (*this)}}
  // expected-warning@-6 {{pset(Post(*c)) = ((null), *a, *b)}}
  // expected-warning@-7 {{pset(Post((return value))) = ((null), *this)}}
  __lifetime_contracts(p7);
  // expected-warning@-1 {{pset(Pre(a)) = ((null), *a)}}
  // expected-warning@-2 {{pset(Pre(b)) = ((null), *b)}}
  // expected-warning@-3 {{pset(Pre(c)) = (*c)}}
  // expected-warning@-4 {{pset(Pre(*c)) = ((invalid))}}
  // expected-warning@-5 {{pset(Post(*c)) = ((null), *a)}}
  __lifetime_contracts(p8);
  // expected-warning@-1 {{pset(Pre(a)) = ((null), *a)}}
  // expected-warning@-2 {{pset(Pre(b)) = ((null), *b)}}
  // expected-warning@-3 {{pset(Pre(c)) = ((null), *c)}}
  // expected-warning@-4 {{pset(Pre(*c)) = ((invalid))}}
  // expected-warning@-5 {{pset(Post(*c)) = ((null), *a)}}
  __lifetime_contracts(p9);
  // expected-warning@-1 {{pset(Pre(a)) = ((null), *a)}}
  // expected-warning@-2 {{pset(Pre(b)) = ((null), *b)}}
  // expected-warning@-3 {{pset(Pre(c)) = (*c)}}
  // expected-warning@-4 {{pset(Pre(*c)) = ((null), **c)}}
  __lifetime_contracts(p10);
  // expected-warning@-1 {{pset(Post((return value))) = ((global))}}
}
} // namespace dump_contracts