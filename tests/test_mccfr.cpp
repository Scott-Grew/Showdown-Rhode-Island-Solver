// The sampled solver: agreement with analytic Kuhn and with full-tree
// CFR on Leduc, and the multi-thread invariants.

#include <catch2/catch_test_macros.hpp>

#include <cmath>
#include <cstdint>
#include <set>
#include <vector>

#include "game/kuhn.hpp"
#include "game/leduc.hpp"
#include "solver/best_response.hpp"
#include "solver/mccfr_solver.hpp"

using namespace cfr::game;
using cfr::solver::exploitability;
using cfr::solver::ExternalSamplingSolver;

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

TEST_CASE("strategy mass equals opponent node visits at every thread count") {
    for (int thread_count : {1, 2, 4}) {
        LeducGame game;
        ExternalSamplingSolver<LeducGame> solver(game, 20260726);
        solver.run_iterations(20000, thread_count);

        double mass = solver.strategy_mass();
        double visits = static_cast<double>(solver.opponent_node_visits());
        REQUIRE(visits > 0.0);
        REQUIRE(std::abs(mass - visits) / visits < 1e-9);
    }
}

TEST_CASE("one thread is byte-identical to the serial reference") {
    LeducGame game;
    ExternalSamplingSolver<LeducGame> single(game, 20260726);
    ExternalSamplingSolver<LeducGame> repeat(game, 20260726);
    single.run_iterations(5000, 1);
    repeat.run_iterations(5000, 1);

    std::vector<double> a(4), b(4);
    for (std::uint32_t index = 0; index < game.infoset_count(); ++index) {
        single.average_strategy_into(index, 2, a);
        repeat.average_strategy_into(index, 2, b);
        REQUIRE(a[0] == b[0]);
        REQUIRE(a[1] == b[1]);
    }
}

TEST_CASE("per-thread streams are distinct") {
    REQUIRE(cfr::solver::scrambled_seed(20260726, 0) == 20260726);
    std::set<std::uint64_t> seen;
    for (std::size_t index = 0; index < 16; ++index) {
        seen.insert(cfr::solver::scrambled_seed(20260726, index));
    }
    REQUIRE(seen.size() == 16);
}
