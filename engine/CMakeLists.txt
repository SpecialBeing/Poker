cmake_minimum_required(VERSION 3.16)
project(poker_engine VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Aggressive optimization for release builds
set(CMAKE_CXX_FLAGS_RELEASE "-O3 -march=native -DNDEBUG")

# ── Library ──────────────────────────────────────────────────
add_library(poker_engine
    src/card.cpp
    src/hand_evaluator.cpp
    src/range.cpp
    src/equity_calculator.cpp
    src/game_state.cpp
)
target_include_directories(poker_engine PUBLIC include)
target_compile_options(poker_engine PRIVATE -Wall -Wextra -Wpedantic)

# Threading support (for equity calculator)
find_package(Threads REQUIRED)
target_link_libraries(poker_engine PUBLIC Threads::Threads)

# ── Demo / test executable ───────────────────────────────────
add_executable(poker_test src/main.cpp)
target_link_libraries(poker_test PRIVATE poker_engine)
