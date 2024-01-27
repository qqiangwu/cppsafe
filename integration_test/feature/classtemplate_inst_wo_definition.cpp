template <class T> void __lifetime_pset(T&&);

template <class T> void __lifetime_contracts(T&&);

namespace stdx {

template <class T> class [[gsl::Owner(int)]] unique_ptr {
    void foo(T*);
};

template <class Sig> class function;

template <class Ret, class... Args>
class function<Ret(Args...)>
{
public:
    Ret operator()(Args...);
};

}

struct Test {
    stdx::function<int*(stdx::unique_ptr<int>*)> mCall;

    void test(stdx::unique_ptr<int>* guard)
    {
        auto* p = mCall(guard);
        __lifetime_pset(p);  // expected-warning {{(**guard)}}
    }
};
