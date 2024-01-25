[![Linux](https://github.com/qqiangwu/cppsafe/actions/workflows/ci-linux.yml/badge.svg?branch=main)](https://github.com/qqiangwu/cppsafe/actions/workflows/ci-linux.yml)
[![macOS](https://github.com/qqiangwu/cppsafe/actions/workflows/ci-macos.yml/badge.svg?branch=main)](https://github.com/qqiangwu/cppsafe/actions/workflows/ci-macos.yml)

# Intro
This is a C++ static analyzer to enforce the lifetime profile:

+ C++ Core Guidelines Lifetime Profile: <https://isocpp.github.io/CppCoreGuidelines/CppCoreGuidelines#SS-lifetime>
+ Spec: <https://github.com/isocpp/CppCoreGuidelines/blob/master/docs/Lifetime.pdf>

The code is based on [Matthias Gehre's fork of LLVM](https://github.com/mgehre/llvm-project).

# Usage
Please make sure conan2 and cmake is avaiable.

## Build
### MacOS + Local LLVM@17
```bash
# Configuration
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release -DLOCAL_CLANG=$(brew --prefix llvm@17)

# Build
cmake --build build -j

# Install (optional)
sudo cmake --install build
```

### With LLVM on conan
```bash
# Install llvm via conan
cd conan
conan export --name=llvm --version=17.0.2 .
cd -

# Configuration
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release

# Build
cmake --build build -j

# Install (optional)
sudo cmake --install build

# Or you can try cppship https://github.com/qqiangwu/cppship
cppship build -r
sudo cppship install
```

## Test
```bash
cd integration_test
bash run_tests.sh
```

You can see `integration_test/example.cpp` for what the lifetime profile can check.

## Use it
### By hand
Assume cppsafe is already installed.

```bash
cppsafe example.cpp -- -std=c++20
```

Pass cppsafe arguments before `--`, pass compiler arguments after `--`.

Note that cppsafe will detect system includes via `c++`, you can override it via the environment variable `CXX`.

```bash
CXX=/opt/homebrew/opt/llvm/bin/clang cppsafe example.cpp -- -std=c++20
```

### With compile\_commands.json
Generally, you should use cppsafe with compile\_commands.json.

Generate compile\_commands.json via cmake, assume it's in `build`. And run:

```bash
cppsafe -p build a.cpp b.cpp c.cpp
```

## Feature test
cppsafe will define `__CPPSAFE__` when compiling your code.

# Tutorial
## Suppress warning
Warnings can be suppressed by function/block/statement/declaration.

```cpp
[[gsl::suppress("lifetime")]]
void foo1(int* p)
{
    *p;
}

void foo2(int* p)
{
    [[gsl::suppress("lifetime")]]
    {
        *p;
        *p;
    }

    [[gsl::suppress("lifetime")]]
    int x = *p;

    [[gsl::suppress("lifetime")]]
    *p;
}
```

## Nullness
Cppsafe will check nullness for pointers. The rules are:

```cpp
void f1(int* p);  // pset(p) = {null, *p}

void f2(vector<int>::iterator p);  // pset(p) = {*p}

using A [[clang::annotate("gsl::lifetime_nonnull")]]= int*;
using B = int*;
void f3(A a, B b);
    // pset(a) = {*a}
    // pset(b) = {null, *b}

// For pointer classes, if is has `operator bool`, then null will be infered
void f4(std::function<void()> f, span<int> s);
    // pset(f) = {null, *f}
    // pset(s) = {*s}
```

# Debug functions
## `__lifetime_pset`
```cpp
template <class T>
void __lifetime_pset(T&&) {}

void foo(int* p)
{
    __lifetime_pset(p);
}
```

## `__lifetime_contracts`
```cpp
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
The following annotations are supported now.

## `gsl::lifetime_const`
+ `[[clang::annotate("gsl::lifetime_const")]]`
+ put it after parameter name or before function signature to make it `lifetime_const`

```cpp
struct [[gsl::Owner(int)]] Dummy
{
    int* Get();

    [[clang::annotate("gsl::lifetime_const")]] void Foo();
};

void Foo(Dummy& p [[clang::annotate("gsl::lifetime_const")]]);
void Foo([[clang::annotate("gsl::lifetime_const")]] Dummy& p);
```

## `gsl::lifetime_in`
+ `[[clang::annotate("gsl::lifetime_in")]]`
+ mark a pointer to pointer or ref to pointer parameter as an input parameter

```cpp
// by default, p is viewed as an output parameter
void Foo(int** p);
void Foo2(int*& p);

void Bar([[clang::annotate("gsl::lifetime_in")]] int** p);
void Bar2([[clang::annotate("gsl::lifetime_in")]] int*& p);
```

## `gsl::pre` and `gsl::post`
We need to use `clang::annotate` to mimic the effects of `[[gsl::pre]]` and `[[gsl::post]]` in the paper.

```cpp
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

## NEW `gsl::lifetime_nonnull`
This annotates a type alias of a raw pointer as nonnull.

```cpp
using A [[clang::annotate("gsl::lifetime_nonnull")]]= int*;
typedef int* B [[clang::annotate("gsl::lifetime_nonnull")]];
```

## `clang::reinitializes`
This is to reset a moved-from object.

```cpp
class Value
{
public:
    [[clang::reinitializes]] void Reset();
};
```

# Difference from the original implementation
## Output variable
### Return check
```cpp
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

You can use `--Wlifetime-output` to enable the check, which enforces that `*out` must be initialized on all paths.

### Precondition inference
A `Pointer*` parameter will be inferred as an output variable, and we will add two implicit preconditions:

+ If Pointer is a raw pointer or has default contructor
    + `pset(p) = {*p}`
    + `pset(*p) = {Invalid}`
+ Otherwise
    + `pset(p) = {*p}`
    + `pset(*p) = {**p}`
