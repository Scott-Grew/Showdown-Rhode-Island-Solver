#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>

#include <limits>
#include <vector>

#include "oracles/best_response.hpp"
#include "oracles/cfr_solver.hpp"
#include "oracles/kuhn.hpp"
#include "tree_walk.hpp"

using namespace cfr::game;
using namespace cfr::solver;

namespace {

void check_kuhn_structure(const StrategyProfile& average) {
    double p0_jack_bet = average.at("P0:J:")[1];
    double p0_queen_bet = average.at("P0:Q:")[1];
    double p0_king_bet = average.at("P0:K:")[1];
    double p1_jack_bet_facing_check = average.at("P1:J:check,")[1];
    double p1_king_bet_facing_check = average.at("P1:K:check,")[1];
    double p1_queen_call_facing_bet = average.at("P1:Q:bet,")[0];

    CHECK(p0_queen_bet == Catch::Approx(0.0).margin(0.01));
    CHECK(p1_king_bet_facing_check == Catch::Approx(1.0).margin(0.01));
    CHECK(p1_jack_bet_facing_check == Catch::Approx(1.0 / 3.0).margin(0.05));
    CHECK(p1_queen_call_facing_bet == Catch::Approx(1.0 / 3.0).margin(0.05));
    CHECK(p0_king_bet == Catch::Approx(3.0 * p0_jack_bet).margin(0.05));
}

}

TEST_CASE("kuhn vanilla cfr reaches the analytic equilibrium") {
    KuhnGame game;
    CfrSolver<KuhnGame> solver(game);

    std::vector<int> checkpoint_iterations = {100, 1000, 10000, 100000};
    double previous_exploitability = std::numeric_limits<double>::infinity();
    int iterations_run = 0;
    StrategyProfile average;
    for (int checkpoint : checkpoint_iterations) {
        solver.run_iterations(checkpoint - iterations_run);
        iterations_run = checkpoint;
        average = solver.average_strategy();
        double current_exploitability = exploitability(game, average);
        CHECK(current_exploitability < previous_exploitability);
        previous_exploitability = current_exploitability;
    }
    REQUIRE(previous_exploitability < 1e-3);
    CHECK(expected_value(game, average, 0) ==
          Catch::Approx(-1.0 / 18.0).margin(1e-3));
    check_kuhn_structure(average);
}
