cmake_minimum_required(VERSION 3.16)
project(poker_engine VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Aggressive optimization for release builds
set(CMAKE_CXX_FLAGS_RELEASE "-O3 -march=native -DNDEBUG")

# ── Core Library ─────────────────────────────────────────────
add_library(poker_engine
    src/card.cpp
    src/hand_evaluator.cpp
    src/range.cpp
    src/equity_calculator.cpp
    src/game_state.cpp
)
target_include_directories(poker_engine PUBLIC include)
target_compile_options(poker_engine PRIVATE -Wall -Wextra -Wpedantic)

find_package(Threads REQUIRED)
target_link_libraries(poker_engine PUBLIC Threads::Threads)

# ── Demo executable ──────────────────────────────────────────
add_executable(poker_demo src/main.cpp)
target_link_libraries(poker_demo PRIVATE poker_engine)

# ── Tests (Google Test) ──────────────────────────────────────
include(FetchContent)
FetchContent_Declare(
    googletest
    GIT_REPOSITORY https://github.com/google/googletest.git
    GIT_TAG        v1.14.0
)
set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(googletest)

enable_testing()

# One test executable per module
set(TEST_MODULES card hand_eval range equity game_state)

foreach(mod ${TEST_MODULES})
    add_executable(test_${mod} tests/test_${mod}.cpp)
    target_link_libraries(test_${mod} PRIVATE poker_engine GTest::gtest_main)
    target_compile_options(test_${mod} PRIVATE -Wall -Wextra)

    # Register with CTest — each test file is a separate CTest entry
    include(GoogleTest)
    gtest_discover_tests(test_${mod} PROPERTIES TIMEOUT 120)
endforeach()
