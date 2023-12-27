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