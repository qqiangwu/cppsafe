template <class T>
struct [[gsl::Owner(T)]] Owner {
    T& get();
    const T& get() const;
};

template <class T>
struct [[gsl::Pointer(T)]] Ptr {
    T& get();
    const T& get() const;
};

template <class A, class B>
struct Pair {
    A first;
    B second;
};

namespace std {

template <class T>
__remove_reference_t(T)&& move(T&& t);

}

template <class T>
void __lifetime_pset(T&&);

void __lifetime_pmap();

template <class T>
void __lifetime_contracts(T&&) {};

#define CPPSAFE_SUPPRESS_LIFETIME [[gsl::suppress("lifetime")]]

#define CPPSAFE_LIFETIME_IN [[clang::annotate("gsl::lifetime_in")]]

#define CPPSAFE_LIFETIME_INOUT [[clang::annotate("gsl::lifetime_inout")]]

#define CPPSAFE_LIFETIME_CONST [[clang::annotate("gsl::lifetime_const")]]

#define CPPSAFE_REINITIALIZES [[clang::reinitializes]]

#define CPPSAFE_PRE(...) [[clang::annotate("gsl::lifetime_pre", __VA_ARGS__)]]

#define CPPSAFE_POST(...) [[clang::annotate("gsl::lifetime_post", __VA_ARGS__)]]

#define CPPSAFE_CAPTURE(...) [[clang::annotate("gsl::lifetime_capture", __VA_ARGS__)]]

#define CPPSAFE_NONNULL [[clang::annotate("gsl::lifetime_nonnull")]]
