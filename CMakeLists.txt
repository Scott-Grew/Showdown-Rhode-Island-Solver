cmake_minimum_required(VERSION 3.20)
project(cfr CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

add_compile_options(-Wall -Wextra -Werror)

include(FetchContent)
FetchContent_Declare(
    Catch2
    GIT_REPOSITORY https://github.com/catchorg/Catch2.git
    GIT_TAG v3.7.1
)
FetchContent_MakeAvailable(Catch2)

# cfr_game is a STATIC library now that Kuhn/Leduc (T3/T4) provide real
# sources. cfr_eval stays header-only (INTERFACE) until the evaluator (T5/T6)
# lands its own sources.
add_library(cfr_game STATIC
    src/game/kuhn.cpp
    src/game/leduc.cpp
)
target_include_directories(cfr_game PUBLIC ${CMAKE_SOURCE_DIR}/src)

add_library(cfr_eval INTERFACE)
target_include_directories(cfr_eval INTERFACE ${CMAKE_SOURCE_DIR}/src)

add_executable(cfr_tests
    tests/test_card.cpp
    tests/test_kuhn.cpp
    tests/test_leduc.cpp
)
target_include_directories(cfr_tests PRIVATE ${CMAKE_SOURCE_DIR}/tests)
target_link_libraries(cfr_tests PRIVATE cfr_game cfr_eval Catch2::Catch2WithMain)

list(APPEND CMAKE_MODULE_PATH ${catch2_SOURCE_DIR}/extras)
include(CTest)
include(Catch)
catch_discover_tests(cfr_tests)

# cfr_bench: no bench sources exist until T7 — gate the target behind their
# existence rather than shipping a placeholder main.
if(EXISTS ${CMAKE_SOURCE_DIR}/bench/bench_eval.cpp)
    add_executable(cfr_bench bench/bench_eval.cpp)
    target_link_libraries(cfr_bench PRIVATE cfr_game cfr_eval)
endif()
