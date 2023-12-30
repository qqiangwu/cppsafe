# Intro
A cpp static analyzer to enforce lifetime profile.

Code is based on https://github.com/mgehre/llvm-project.

# Build
Please make sure conan2 and cmake is avaiable.

```bash
# Install llvm via conan
cd conan
conan export --name=llvm --version=17.0.2 .
cd -

# Build
cmake -S . -B build

# Or you can try cppship https://github.com/qqiangwu/cppship
cppship build
```

# Debug functions
```
template <class T>
void __lifetime_contracts(T&&);

void Foo();

struct Dummy { int Foo(int*); };

__lifetime_contracts(&Foo);

// __lifetime_contracts will derive the actual function from the call
// note a return value is required to make semantics check pass
__lifetime_contracts(Dummy{}.Foo(nullptr));
```