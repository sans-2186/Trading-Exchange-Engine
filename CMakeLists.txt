cmake_minimum_required(VERSION 3.20)

project(MercuryExchangeEngine
    VERSION 0.1.0
    DESCRIPTION "Modular, low-latency electronic exchange simulator"
    LANGUAGES CXX
)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")
include(CompilerWarnings)

# vcpkg manifest mode (optional — falls back to FetchContent for GTest)
if(DEFINED ENV{VCPKG_ROOT})
    set(CMAKE_TOOLCHAIN_FILE "$ENV{VCPKG_ROOT}/scripts/buildsystems/vcpkg.cmake"
        CACHE STRING "vcpkg toolchain file")
endif()

# --- mercury_core static library ---
add_library(mercury_core STATIC
    src/core/version.cpp
)

target_include_directories(mercury_core
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include>
)

mercury_set_compiler_warnings(mercury_core)

# --- mercury_cli executable ---
add_executable(mercury_cli src/cli/main.cpp)
target_link_libraries(mercury_cli PRIVATE mercury_core)
mercury_set_compiler_warnings(mercury_cli)

# --- Tests ---
enable_testing()

find_package(GTest QUIET)
if(NOT GTest_FOUND)
    include(FetchContent)
    FetchContent_Declare(
        googletest
        GIT_REPOSITORY https://github.com/google/googletest.git
        GIT_TAG v1.14.0
    )
    set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
    FetchContent_MakeAvailable(googletest)
endif()

add_executable(mercury_tests
    tests/unit/test_smoke.cpp
)

target_link_libraries(mercury_tests PRIVATE mercury_core GTest::gtest_main)
mercury_set_compiler_warnings(mercury_tests)

include(GoogleTest)
gtest_discover_tests(mercury_tests)

# --- Benchmarks (stub — Google Benchmark added in Phase 4) ---
add_executable(mercury_bench benchmarks/bench_stub.cpp)
target_link_libraries(mercury_bench PRIVATE mercury_core)
mercury_set_compiler_warnings(mercury_bench)

# --- Qt Dashboard (Phase 8 — disabled by default) ---
option(MERCURY_BUILD_DASHBOARD "Build Qt engineering dashboard" OFF)
if(MERCURY_BUILD_DASHBOARD)
    add_subdirectory(apps/dashboard)
endif()
