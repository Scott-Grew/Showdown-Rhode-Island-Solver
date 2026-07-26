#include <catch2/catch_test_macros.hpp>
#include <cstdio>
#include <fstream>
#include <ios>
#include <iterator>
#include <stdexcept>
#include <string>
#include <vector>

#include "game/leduc.hpp"
#include "solver/best_response.hpp"
#include "solver/cfr_solver.hpp"

using namespace cfr::game;
using namespace cfr::solver;

TEST_CASE("V19: leduc cfr+ rejects checkpoint from a mismatched game") {
    LeducGame game;
    CfrPlus<cfr::game::LeducGame> solver(game);
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

    REQUIRE_THROWS_AS(CfrPlus<cfr::game::LeducGame>::load_checkpoint(game, checkpoint_path), std::runtime_error);
    std::remove(checkpoint_path.c_str());
}

TEST_CASE("V22: leduc cfr+ rejects a truncated checkpoint") {
    LeducGame game;
    CfrPlus<cfr::game::LeducGame> solver(game);
    solver.run_iterations(10);

    const std::string checkpoint_path = "test_checkpoint_leduc_truncated.tmp";
    solver.save_checkpoint(checkpoint_path);

    std::ifstream original_input(checkpoint_path, std::ios::binary);
    std::string contents((std::istreambuf_iterator<char>(original_input)), std::istreambuf_iterator<char>());
    original_input.close();

    std::ofstream truncated_output(checkpoint_path, std::ios::binary | std::ios::trunc);
    truncated_output << contents.substr(0, contents.size() * 2 / 3);
    truncated_output.close();

    REQUIRE_THROWS_AS(CfrPlus<cfr::game::LeducGame>::load_checkpoint(game, checkpoint_path), std::runtime_error);
    std::remove(checkpoint_path.c_str());
}

TEST_CASE("V23: leduc rejects a checkpoint written by the other solver policy") {
    LeducGame game;
    VanillaCfr<cfr::game::LeducGame> vanilla_solver(game);
    vanilla_solver.run_iterations(10);

    const std::string checkpoint_path = "test_checkpoint_leduc_vanilla_policy.tmp";
    vanilla_solver.save_checkpoint(checkpoint_path);

    REQUIRE_THROWS_AS(CfrPlus<cfr::game::LeducGame>::load_checkpoint(game, checkpoint_path), std::runtime_error);
    REQUIRE_NOTHROW(VanillaCfr<cfr::game::LeducGame>::load_checkpoint(game, checkpoint_path));
    std::remove(checkpoint_path.c_str());
}

TEST_CASE("V25: leduc solvers exercise every path at sanitizer-affordable iteration counts") {
    LeducGame game;

    VanillaCfr<cfr::game::LeducGame> vanilla_solver(game);
    vanilla_solver.run_iterations(25);
    REQUIRE(exploitability(game, vanilla_solver.average_strategy()) >= 0.0);
    REQUIRE(!vanilla_solver.current_strategy().empty());

    CfrPlus<cfr::game::LeducGame> cfr_plus_solver(game);
    cfr_plus_solver.run_iterations(25);
    StrategyProfile direct_strategy = cfr_plus_solver.average_strategy();
    REQUIRE(exploitability(game, direct_strategy) >= 0.0);
    REQUIRE(!cfr_plus_solver.current_strategy().empty());

    CfrPlus<cfr::game::LeducGame> first_half_solver(game);
    first_half_solver.run_iterations(12);
    const std::string checkpoint_path = "test_checkpoint_leduc_smoke.tmp";
    first_half_solver.save_checkpoint(checkpoint_path);

    CfrPlus<cfr::game::LeducGame> resumed_solver = CfrPlus<cfr::game::LeducGame>::load_checkpoint(game, checkpoint_path);
    resumed_solver.run_iterations(13);
    std::remove(checkpoint_path.c_str());

    REQUIRE(resumed_solver.average_strategy() == direct_strategy);
    REQUIRE(resumed_solver.iterations_run() == cfr_plus_solver.iterations_run());
}
