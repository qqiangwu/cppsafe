namespace std {

template <class T>
class [[gsl::Owner(T)]] vector {
public:
    using iterator = T*;
    using value_type = T;
    using reference_type = T&;

    iterator begin();
    iterator end();

    reference_type operator[](int x);

    reference_type back();
    void pop_back();

    ~vector();
};

template <class T>
__remove_reference_t(T)&& move(T&& t);

}

struct Dummy { void foo(); };

template <class T>
void __lifetime_pset(T&&);

void test_loop()
{
    std::vector<Dummy> vec;
    __lifetime_pset(vec);  // expected-warning {{pset(vec) = (*vec)}}

    for (auto& x: vec) {
        auto y = std::move(x);
    }

    __lifetime_pset(vec);  // expected-warning {{pset(vec) = (*vec)}}
}

void test_loop_back()
{
    std::vector<Dummy> vec;

    for (auto& x: vec) {
        auto y = std::move(x);  // expected-note {{moved here}}
        x.foo();  // expected-warning {{dereferencing a dangling pointer}}
    }
}

void test_iterator()
{
    std::vector<Dummy> vec;

    for (auto it = vec.begin(), end = vec.end(); it != end; ++it) {
        auto y = std::move(*it);
    }
}

void test_index()
{
    std::vector<Dummy> vec;

    for (int i = 0; i < 10; ++i) {
        auto y = std::move(vec[i]);
    }
}

void test_back()
{
    std::vector<Dummy> vec;

    auto x = std::move(vec.back());

    vec.pop_back();
}
