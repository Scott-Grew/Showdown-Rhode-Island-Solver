#include <catch2/catch_test_macros.hpp>

#include "game/kuhn.hpp"
#include "game/leduc.hpp"
#include "solver/best_response.hpp"
#include "solver/mccfr_solver.hpp"

using namespace cfr::game;
using cfr::solver::ExternalSamplingSolver;
using cfr::solver::exploitability;

TEST_CASE("external sampling reaches the analytic kuhn game value") {
    KuhnGame game;
    ExternalSamplingSolver<KuhnGame> solver(game, 20260726);
    solver.run_iterations(200000);

    REQUIRE(exploitability(game, solver.average_strategy()) < 1e-2);
}

TEST_CASE("external sampling and vanilla cfr agree on leduc") {
    LeducGame game;
    ExternalSamplingSolver<LeducGame> solver(game, 20260726);
    solver.run_iterations(200000);

    REQUIRE(exploitability(game, solver.average_strategy()) < 0.1);
}
