[![Linux](https://github.com/qqiangwu/cppsafe/actions/workflows/ci-linux.yml/badge.svg?branch=main)](https://github.com/qqiangwu/cppsafe/actions/workflows/ci-linux.yml)
[![macOS](https://github.com/qqiangwu/cppsafe/actions/workflows/ci-macos.yml/badge.svg?branch=main)](https://github.com/qqiangwu/cppsafe/actions/workflows/ci-macos.yml)

# Intro
A cpp static analyzer to enforce lifetime profile.

Code is based on https://github.com/mgehre/llvm-project.

+ Cpp core guidelines Lifetime Profile: https://isocpp.github.io/CppCoreGuidelines/CppCoreGuidelines#SS-lifetime
+ Spec: https://github.com/isocpp/CppCoreGuidelines/blob/master/docs/Lifetime.pdf

# Build
Please make sure conan2 and cmake is avaiable.

```bash
# Install llvm via conan
cd conan
conan export --name=llvm --version=17.0.2 .
cd -

# Build
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release

# If you are using MacOS and already have llvm@17 installed
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release -DLOCAL_CLANG=$(brew --prefix llvm@17)

# Test
cd integration_test
bash run_tests.sh

# Or you can try cppship https://github.com/qqiangwu/cppship
cppship build -r
```

# Debug functions
```C++
template <class T>
void __lifetime_contracts(T&&) {}

void Foo();

struct Dummy {
    int Foo(int*);
    void Bar(int*);
};

__lifetime_contracts(&Foo);

// __lifetime_contracts will derive the actual function from the call
// note a return value is required to make semantics check pass
__lifetime_contracts(Dummy{}.Foo(nullptr));

__lifetime_contracts([]{
    Dummy d;
    d.Bar(nullptr);  // <- will be inspected
});
```

# Annotations
The following annotations are supported now

## gsl::lifetime_const
+ ```[[clang::annotate("gsl::lifetime_const")]]```
+ put it after parameter name or before function signature to make *this lifetime_const

```C++
struct [[gsl::Owner(int)]] Dummy
{
    int* Get();

    [[clang::annotate("gsl::lifetime_const")]] void Foo();
};

void Foo(Dummy& p [[clang::annotate("gsl::lifetime_const")]]);
void Foo([[clang::annotate("gsl::lifetime_const")]] Dummy& p);
```

## gsl::lifetime_in
+ ```[[clang::annotate("gsl::lifetime_in")]]```
+ mark a pointer to pointer or ref to pointer parameter as an input parameter

```C++
// by default, ps is viewed as an output parameter
void Foo(int** p);
void Foo2(int*& p);

void Bar([[clang::annotate("gsl::lifetime_in")]] int** p);
void Bar2([[clang::annotate("gsl::lifetime_in")]] int*& p);
```

## gsl::pre or gsl::post
We need to use clang::annotate to mimic the effects of `[[gsl::pre]]` and `[[gsl::post]]` in the paper

```C++
constexpr int Return = 0;
constexpr int Global = 1;

struct Test
{
    [[clang::annotate("gsl::lifetime_pre", "*z", Global)]]
    [[clang::annotate("gsl::lifetime_post", Return, "x", "y", "*z", "*this")]]
    [[clang::annotate("gsl::lifetime_post", "*z", "x")]]
    int* Foo(int* x [[clang::annotate("gsl::lifetime_pre", Global)]], int* y, int** z);
};
```

## clang::reinitializes
Reset a moved-from object.

```C++
class Value
{
public:
    [[clang::reinitializes]] void Reset();
};
```

# Difference from the original implementation
## Output variable
### Return check
```C++
bool foo(int** out)
{
    if (cond) {
        // pset(*out) = {invalid}, we allow it, since it's so common
        return false;
    }

    *out = new int{};
    return true;
}
```

You can use `--Wlifetime-output` to enable the check, which enforce `*out` is initialized in all paths.

### Precondition inference
A `Pointer*` parameter will be infered as an output variable, and we will add two implicit precondition:

+ If Pointer is a raw pointer or has default contructor
    + `pset(p) = {*p}`
    + `pset(*p) = {Invalid}`
+ Otherwise
    + `pset(p) = {*p}`
    + `pset(*p) = {**p}`