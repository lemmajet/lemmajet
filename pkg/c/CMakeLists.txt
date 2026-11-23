cmake_minimum_required(VERSION 3.16)
project(lemmajet-c VERSION 0.8.1 LANGUAGES C)

set(CMAKE_C_STANDARD 99)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS OFF)

if(MSVC)
    add_compile_options(/W4 /WX)
else()
    add_compile_options(-Wall -Wextra -Werror -pedantic)
endif()

# Header-only interface library
add_library(lemmajet INTERFACE)
target_include_directories(lemmajet INTERFACE ${CMAKE_CURRENT_SOURCE_DIR})

# Standalone implementation object / static / shared library
set(LEMMAJET_IMPL_SRC ${CMAKE_CURRENT_BINARY_DIR}/lemmajet_impl.c)
file(WRITE ${LEMMAJET_IMPL_SRC} "#define LEMMAJET_IMPLEMENTATION\n#define LEMMAJET_ENABLE_SNOWBALL_COMPAT 1\n#include \"lemmajet.h\"\n")

add_library(lemmajet_static STATIC ${LEMMAJET_IMPL_SRC})
target_link_libraries(lemmajet_static PUBLIC lemmajet)

add_library(lemmajet_shared SHARED ${LEMMAJET_IMPL_SRC})
target_link_libraries(lemmajet_shared PUBLIC lemmajet)
set_target_properties(lemmajet_shared PROPERTIES POSITION_INDEPENDENT_CODE ON)

# Tests
enable_testing()
if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/tests/test_parity.c)
    add_executable(test_parity tests/test_parity.c)
    target_link_libraries(test_parity PRIVATE lemmajet)
    add_test(NAME test_parity COMMAND test_parity)
endif()

# Benchmark
if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/benchmarks/bench_c.c)
    add_executable(bench_c benchmarks/bench_c.c)
    target_link_libraries(bench_c PRIVATE lemmajet)
endif()

