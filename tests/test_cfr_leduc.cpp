// Runs both full-tree solvers on Leduc briefly, so sanitizer builds
// cover those code paths.

#include <catch2/catch_test_macros.hpp>

#include "game/leduc.hpp"
#include "solver/best_response.hpp"
#include "solver/cfr_solver.hpp"

using namespace cfr::game;
using namespace cfr::solver;

TEST_CASE(
    "leduc solvers run every path at sanitizer-affordable iteration counts") {
    LeducGame game;

    VanillaCfr<cfr::game::LeducGame> vanilla_solver(game);
    vanilla_solver.run_iterations(25);
    REQUIRE(exploitability(game, vanilla_solver.average_strategy()) >= 0.0);

    CfrPlus<cfr::game::LeducGame> cfr_plus_solver(game);
    cfr_plus_solver.run_iterations(25);
    REQUIRE(exploitability(game, cfr_plus_solver.average_strategy()) >= 0.0);
    REQUIRE(cfr_plus_solver.iterations_run() == 25);
}
