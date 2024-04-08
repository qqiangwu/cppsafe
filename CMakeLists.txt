cmake_minimum_required(VERSION 3.17)
project(cppsafe VERSION 0.1.0)

include(CTest)
enable_testing()

if(CMAKE_EXPORT_COMPILE_COMMANDS)
    set(CMAKE_CXX_STANDARD_INCLUDE_DIRECTORIES ${CMAKE_CXX_IMPLICIT_INCLUDE_DIRECTORIES})
endif()

# cpp options
if(NOT (CMAKE_CXX_COMPILER_ID STREQUAL "MSVC"))
	add_compile_options(-Wall)
	add_compile_options(-Wextra)
	add_compile_options(-Werror)
	add_compile_options(-Wno-unused-parameter)
	add_compile_options(-Wno-missing-field-initializers)
endif()

if((CMAKE_SYSTEM_NAME STREQUAL "Linux" OR CMAKE_CXX_COMPILER_ID STREQUAL "Clang" OR CMAKE_CXX_COMPILER_ID STREQUAL "AppleClang"))
endif()


# profile cpp options
if(NOT (CMAKE_CXX_COMPILER_ID STREQUAL "MSVC"))
	add_compile_options($<$<CONFIG:Debug>:-fsanitize=undefined>)
	add_link_options($<$<CONFIG:Debug>:-fsanitize=undefined>)
	message(STATUS "Enable ubsan")
endif()

if((CMAKE_SYSTEM_NAME STREQUAL "Linux" OR CMAKE_CXX_COMPILER_ID STREQUAL "Clang" OR CMAKE_CXX_COMPILER_ID STREQUAL "AppleClang"))
	add_compile_options($<$<CONFIG:Debug>:-fsanitize=address>)
	add_link_options($<$<CONFIG:Debug>:-fsanitize=address>)
	message(STATUS "Enable asan")
endif()


# cpp std
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED On)
set(CMAKE_CXX_EXTENSIONS Off)

# Dependency management
if(LOCAL_CLANG)
	execute_process(COMMAND grep -v llvm conanfile.txt.tpl OUTPUT_FILE conanfile.txt RESULT_VARIABLE return_code)
	if(NOT ${return_code} EQUAL "0")
		message(FATAL_ERROR "remove llvm dependency failed")
	endif()

	message(STATUS "Use local clang in ${LOCAL_CLANG} (instead of conan)")
else()
	execute_process(COMMAND cp conanfile.txt.tpl conanfile.txt RESULT_VARIABLE return_code)

	message(STATUS "Use llvm from conan")
endif()

include(cmake/conan.cmake)

# Package finders
if(LOCAL_CLANG)
	set(Clang_DIR "${LOCAL_CLANG}/lib/cmake/clang/")
	find_package(Clang REQUIRED)
else()
	find_package(LLVM REQUIRED)
endif()
find_package(Microsoft.GSL REQUIRED)
find_package(fmt REQUIRED)
find_package(range-v3 REQUIRED)
find_package(nextsilicon-cpp-subprocess REQUIRED)

add_library(cppsafe_deps INTERFACE)
if(LOCAL_CLANG)
	target_include_directories(cppsafe_deps SYSTEM INTERFACE ${CLANG_INCLUDE_DIRS})
endif()
target_link_libraries(cppsafe_deps INTERFACE clangTooling)
target_link_libraries(cppsafe_deps INTERFACE Microsoft.GSL::GSL)
target_link_libraries(cppsafe_deps INTERFACE fmt::fmt)
target_link_libraries(cppsafe_deps INTERFACE range-v3::range-v3)
target_link_libraries(cppsafe_deps INTERFACE nextsilicon-cpp-subprocess::nextsilicon-cpp-subprocess)

# LIB
add_library(cppsafe_lib ${CMAKE_SOURCE_DIR}/lib/AstConsumer.cpp
${CMAKE_SOURCE_DIR}/lib/lifetime/Lifetime.cpp
${CMAKE_SOURCE_DIR}/lib/lifetime/LifetimeAttrHandling.cpp
${CMAKE_SOURCE_DIR}/lib/lifetime/LifetimePsetBuilder.cpp
${CMAKE_SOURCE_DIR}/lib/lifetime/LifetimeTypeCategory.cpp
${CMAKE_SOURCE_DIR}/lib/lifetime/Debug.cpp
${CMAKE_SOURCE_DIR}/lib/lifetime/contract/CallVisitor.cpp
${CMAKE_SOURCE_DIR}/lib/lifetime/contract/Parser.cpp
${CMAKE_SOURCE_DIR}/lib/lifetime/type/Aggregate.cpp
)
set_target_properties(cppsafe_lib PROPERTIES OUTPUT_NAME "cppsafe")

target_include_directories(cppsafe_lib PUBLIC ${CMAKE_SOURCE_DIR}/include)

target_link_libraries(cppsafe_lib PUBLIC cppsafe_deps)

# BIN
add_executable(cppsafe ${CMAKE_SOURCE_DIR}/src/main.cpp)

target_link_libraries(cppsafe PRIVATE cppsafe_lib)
target_link_libraries(cppsafe PRIVATE cppsafe_deps)

target_compile_definitions(cppsafe PRIVATE CPPSAFE_VERSION="${PROJECT_VERSION}")

install(TARGETS cppsafe)

# Dev Package finders
add_library(cppsafe_dev_deps INTERFACE)
target_link_libraries(cppsafe_dev_deps INTERFACE cppsafe_deps)

# Groups
add_custom_target(cppship_group_binaries DEPENDS cppsafe cppsafe_lib)
add_custom_target(cppship_group_examples DEPENDS )
add_custom_target(cppship_group_benches DEPENDS )
add_custom_target(cppship_group_tests DEPENDS )

# Footer
set(CPACK_PROJECT_NAME ${PROJECT_NAME})
set(CPACK_PROJECT_VERSION ${PROJECT_VERSION})
include(CPack)
