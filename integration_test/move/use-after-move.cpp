// ARGS: -Wlifetime-move

#include <vector>

struct Dummy {
    void foo();
    void reset();

    void use() &&;
};

template <class T>
void useRef(const T&);

template <class T>
void usePtr(const T*);

template <class T>
void useForwardRef(T&& ref);

template <class T>
void moveIt(T&&);

void test_owner()
{
    std::vector<int> v1;
    std::vector<int> v2;

    auto it = v1.begin();
    v2 = std::move(v1);
    // expected-note@-1 {{modified here}}
    // expected-note@-2 {{moved here}}
    // expected-note@-3 {{moved here}}
    // expected-note@-4 {{moved here}}
    // expected-note@-5 {{moved here}}

    *it;  // expected-warning {{passing a dangling pointer as argument}}
    v1.front();  // expected-warning {{use a moved-from object}}
    useRef(v1);  // expected-warning {{use a moved-from object}}
    usePtr(&v1);  // expected-warning {{use a moved-from object}}
    v2 = v1;  // expected-warning {{use a moved-from object}}

    v1 = {};
    v1.front();
    useRef(v1);
    usePtr(&v1);

    moveIt(std::move(v1));  // expected-note {{moved here}}
    v1.front();  // expected-warning {{use a moved-from object}}

    v1.clear();
    v1.front();
}

void test_value()
{
    Dummy v1;
    Dummy v2;
    auto it = &v1;
    v2 = std::move(v1);
    // expected-note@-1 {{moved here}}
    // expected-note@-2 {{moved here}}
    // expected-note@-3 {{moved here}}
    // expected-note@-4 {{moved here}}
    // expected-note@-5 {{moved here}}
    // expected-note@-6 {{moved here}}
    // expected-note@-7 {{moved here}}

    *it;  // expected-warning {{dereferencing a dangling pointer}}
    v1.foo();  // expected-warning {{use a moved-from object}}
    useRef(v1);  // expected-warning {{use a moved-from object}}
    usePtr(&v1);  // expected-warning {{use a moved-from object}}
    useForwardRef(v1);  // expected-warning {{use a moved-from object}}
    useForwardRef(&v1);  // expected-warning {{use a moved-from object}}
    v2 = v1;  // expected-warning {{use a moved-from object}}

    v1 = {};
    v1.foo();
    useRef(v1);
    usePtr(&v1);

    moveIt(std::move(v1));  // expected-note {{moved here}}
    v1.foo();  // expected-warning {{use a moved-from object}}

    v1.reset();
    v1.foo();

    std::move(v1).use();  // expected-note {{moved here}}
    v1.foo();  // expected-warning {{use a moved-from object}}
}

void test_constructor()
{
    std::vector<int> v1;
    std::vector<int> v2(std::move(v1));  // expected-note {{moved here}}
    std::vector<int> v3 = std::move(v2);  // expected-note {{moved here}}
    std::vector<int> v4{std::move(v3)};  // expected-note {{moved here}}

    useRef(v1);  // expected-warning {{use a moved-from object}}
    useRef(v2);  // expected-warning {{use a moved-from object}}
    useRef(v3);  // expected-warning {{use a moved-from object}}
    useRef(v4);

    Dummy d1;
    Dummy d2(std::move(d1));  // expected-note {{moved here}}
    Dummy d3 = std::move(d2);  // expected-note {{moved here}}
    Dummy d4{std::move(d3)};  // expected-note {{moved here}}

    useRef(d1);  // expected-warning {{use a moved-from object}}
    useRef(d2);  // expected-warning {{use a moved-from object}}
    useRef(d3);  // expected-warning {{use a moved-from object}}
    useRef(d4);
}