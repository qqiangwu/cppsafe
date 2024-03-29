[requires]
range-v3/0.12.0
fmt/10.1.1
ms-gsl/4.0.0
llvm/17.0.2
nextsilicon-cpp-subprocess/2.0.2

[test_requires]
gtest/cci.20210126
benchmark/1.7.1

[generators]
CMakeDeps

[options]
llvm/*:conan_center_index_limits=False
llvm/*:with_project_clang=True
