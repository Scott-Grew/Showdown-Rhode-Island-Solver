cmake_minimum_required(VERSION 3.20)
project(cfr CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

add_compile_options(-Wall -Wextra -Werror)

include(CheckCXXCompilerFlag)
check_cxx_compiler_flag(-mavx2 CFR_HAS_AVX2_FLAG)
if(CFR_HAS_AVX2_FLAG)
    add_compile_options(-mavx2)
endif()

include(FetchContent)
set(FETCHCONTENT_BASE_DIR ${CMAKE_SOURCE_DIR}/.deps CACHE PATH "Shared dependency cache")
FetchContent_Declare(
    Catch2
    GIT_REPOSITORY https://github.com/catchorg/Catch2.git
    GIT_TAG v3.7.1
    GIT_SHALLOW TRUE
)
FetchContent_MakeAvailable(Catch2)

# cfr_game and cfr_eval are STATIC libraries now that Kuhn/Leduc (T3/T4) and
# the evaluator (T5/T6) provide real sources.
add_library(cfr_game STATIC
    src/game/kuhn.cpp
    src/game/leduc.cpp
    src/game/rhode_island.cpp
)
target_include_directories(cfr_game PUBLIC ${CMAKE_SOURCE_DIR}/src)

add_library(cfr_eval STATIC
    src/eval/eval3.cpp
)
target_include_directories(cfr_eval PUBLIC ${CMAKE_SOURCE_DIR}/src)

# Naive 5-card reference: obviously-correct, deliberately unoptimized,
# shares no code with cfr_eval's fast path. Test-only -- linked into
# cfr_tests alone so no cfr_eval consumer ever pulls it in.
add_library(cfr_eval_naive_ref STATIC
    src/eval/naive_ref.cpp
)
target_include_directories(cfr_eval_naive_ref PUBLIC ${CMAKE_SOURCE_DIR}/src)

add_library(cfr_solver STATIC
    src/solver/strategy.cpp
    src/solver/best_response.cpp
    src/solver/rih_best_response.cpp
)
target_include_directories(cfr_solver PUBLIC ${CMAKE_SOURCE_DIR}/src)
target_link_libraries(cfr_solver PUBLIC cfr_game cfr_eval)

add_library(cfr_grader STATIC
    src/grader/gsi_strategy.cpp
)
target_include_directories(cfr_grader PUBLIC ${CMAKE_SOURCE_DIR}/src)
target_link_libraries(cfr_grader PUBLIC cfr_game)
target_compile_definitions(cfr_grader PUBLIC CFR_GSI_DATA_DIR="${CMAKE_SOURCE_DIR}/data/gsi")

add_executable(cfr_play
    src/demo/play_rih.cpp
)
target_link_libraries(cfr_play PRIVATE cfr_game cfr_eval cfr_grader)

add_executable(cfr_measure
    src/measure/rih_exploitability.cpp
)
target_link_libraries(cfr_measure PRIVATE cfr_game cfr_eval cfr_solver)

add_executable(cfr_tests
    tests/test_card.cpp
    tests/test_kuhn.cpp
    tests/test_leduc.cpp
    tests/test_rhode_island.cpp
    tests/test_eval3.cpp
    tests/test_regret_matching.cpp
    tests/test_best_response.cpp
    tests/test_cfr_kuhn.cpp
    tests/test_cfr_leduc.cpp
    tests/test_game_concept.cpp
    tests/test_state.cpp
    tests/test_gsi_strategy.cpp
    tests/test_gsi_grader.cpp
    tests/test_mccfr.cpp
)
target_include_directories(cfr_tests PRIVATE ${CMAKE_SOURCE_DIR}/tests)
target_link_libraries(cfr_tests PRIVATE cfr_game cfr_eval cfr_eval_naive_ref cfr_solver cfr_grader Catch2::Catch2WithMain)

list(APPEND CMAKE_MODULE_PATH ${catch2_SOURCE_DIR}/extras)
include(CTest)
include(Catch)
catch_discover_tests(cfr_tests)

