namespace std {

template <class Sig>
class function
{
public:
    template <class Fp>
    function(Fp fp);

    template <class Fp>
    function& operator=(Fp&&);

    function& operator=(decltype(nullptr));

    operator bool();
};

}

struct Callable {};

template <class T>
void __lifetime_pset(T&&) {}

void test_pointer()
{
    std::function<void()> f(Callable{});
    __lifetime_pset(f);  // expected-warning {{((global))}}

    f = nullptr;
    __lifetime_pset(f);  // expected-warning {{((null))}}

    f = Callable{};
    __lifetime_pset(f);  // expected-warning {{((global))}}
}