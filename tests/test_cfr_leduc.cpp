#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>

#include <limits>
#include <vector>

#include "game/leduc.hpp"
#include "solver/best_response.hpp"
#include "solver/cfr_plus.hpp"
#include "solver/vanilla_cfr.hpp"

using namespace cfr::game;
using namespace cfr::solver;

TEST_CASE("V20: leduc cfr+ deterministic — two runs identical") {
    LeducGame game;
    CfrPlus solver_a(game);
    solver_a.run_iterations(1000);
    CfrPlus solver_b(game);
    solver_b.run_iterations(1000);
    REQUIRE(solver_a.average_strategy() == solver_b.average_strategy());
}

TEST_CASE("V18: leduc cfr+ exploitability decreases across decade checkpoints") {
    LeducGame game;
    CfrPlus solver(game);

    std::vector<int> checkpoint_iterations = {100, 1000, 3000, 10000};
    double previous_exploitability = std::numeric_limits<double>::infinity();
    int iterations_run = 0;
    for (int checkpoint : checkpoint_iterations) {
        solver.run_iterations(checkpoint - iterations_run);
        iterations_run = checkpoint;
        double current_exploitability = exploitability(game, solver.average_strategy());
        CHECK(current_exploitability < previous_exploitability);
        previous_exploitability = current_exploitability;
    }
}

TEST_CASE("V17: leduc cfr+ exploitability <= vanilla cfr at matched checkpoints") {
    LeducGame game;
    CfrPlus cfr_plus_solver(game);
    VanillaCfr vanilla_solver(game);

    std::vector<int> checkpoint_iterations = {100, 1000, 3000, 10000};
    int iterations_run = 0;
    for (int checkpoint : checkpoint_iterations) {
        int delta = checkpoint - iterations_run;
        cfr_plus_solver.run_iterations(delta);
        vanilla_solver.run_iterations(delta);
        iterations_run = checkpoint;

        double cfr_plus_exploitability = exploitability(game, cfr_plus_solver.average_strategy());
        double vanilla_exploitability = exploitability(game, vanilla_solver.average_strategy());
        CHECK(cfr_plus_exploitability <= vanilla_exploitability);
    }
}
