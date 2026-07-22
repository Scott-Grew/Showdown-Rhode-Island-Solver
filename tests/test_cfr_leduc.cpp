#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>

#include <cstdio>
#include <fstream>
#include <limits>
#include <stdexcept>
#include <string>
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

TEST_CASE("V19: leduc cfr+ checkpoint round-trip is byte-identical to direct run") {
    LeducGame game;

    CfrPlus direct_solver(game);
    direct_solver.run_iterations(1000);
    StrategyProfile direct_strategy = direct_solver.average_strategy();

    CfrPlus first_half_solver(game);
    first_half_solver.run_iterations(500);
    const std::string checkpoint_path = "test_checkpoint_leduc_cfr_plus.tmp";
    first_half_solver.save_checkpoint(checkpoint_path);

    CfrPlus resumed_solver = CfrPlus::load_checkpoint(game, checkpoint_path);
    resumed_solver.run_iterations(500);
    std::remove(checkpoint_path.c_str());

    REQUIRE(resumed_solver.average_strategy() == direct_strategy);
    REQUIRE(resumed_solver.iterations_run() == direct_solver.iterations_run());
}

TEST_CASE("V19: leduc cfr+ rejects checkpoint from a mismatched game") {
    LeducGame game;
    CfrPlus solver(game);
    solver.run_iterations(10);

    const std::string checkpoint_path = "test_checkpoint_leduc_cfr_plus_mismatched.tmp";
    solver.save_checkpoint(checkpoint_path);

    std::ifstream checkpoint_input(checkpoint_path);
    std::vector<std::string> checkpoint_lines;
    std::string checkpoint_line;
    while (std::getline(checkpoint_input, checkpoint_line)) {
        checkpoint_lines.push_back(checkpoint_line);
    }
    checkpoint_input.close();

    for (std::string& stored_line : checkpoint_lines) {
        if (stored_line.rfind("infoset_count ", 0) == 0) {
            stored_line = "infoset_count " + std::to_string(game.infoset_count() + 1);
        }
    }

    std::ofstream checkpoint_output(checkpoint_path);
    for (const std::string& stored_line : checkpoint_lines) {
        checkpoint_output << stored_line << '\n';
    }
    checkpoint_output.close();

    REQUIRE_THROWS_AS(CfrPlus::load_checkpoint(game, checkpoint_path), std::runtime_error);
    std::remove(checkpoint_path.c_str());
}
