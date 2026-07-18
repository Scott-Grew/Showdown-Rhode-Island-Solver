#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>

#include <cstddef>
#include <limits>
#include <vector>

#include "game/kuhn.hpp"
#include "solver/best_response.hpp"
#include "solver/strategy.hpp"
#include "solver/vanilla_cfr.hpp"

using namespace cfr::game;
using namespace cfr::solver;

namespace {

// Test-only verification instrument, deliberately duplicated from
// test_best_response.cpp rather than shared (same rationale as that file's
// header comment: an obviously-correct recursive walk to check the
// production convergence gates against, not to be reused by them).
double expected_value(const Game& game, const StrategyProfile& profile, Player player, const State& state) {
    if (game.is_terminal(state)) return game.terminal_utility(state, player);

    if (game.is_chance(state)) {
        double value = 0.0;
        for (auto& [action, probability] : game.chance_outcomes(state))
            value += probability * expected_value(game, profile, player, game.apply_action(state, action));
        return value;
    }

    std::vector<Action> actions = game.legal_actions(state);
    auto profile_entry = profile.find(game.infoset_key(state));
    double uniform_probability = 1.0 / static_cast<double>(actions.size());

    double value = 0.0;
    for (std::size_t i = 0; i < actions.size(); ++i) {
        double action_probability = profile_entry != profile.end() ? profile_entry->second[i] : uniform_probability;
        value += action_probability * expected_value(game, profile, player, game.apply_action(state, actions[i]));
    }
    return value;
}

double expected_value(const Game& game, const StrategyProfile& profile, Player player) {
    return expected_value(game, profile, player, game.initial_state());
}

}  // namespace

TEST_CASE("kuhn: exploitability < 1e-3 after 1e5 iterations") {     // V11
    KuhnGame game;
    VanillaCfr solver(game);
    solver.run_iterations(100000);
    REQUIRE(exploitability(game, solver.average_strategy()) < 1e-3);
}

TEST_CASE("kuhn: game value converges to -1/18") {                  // V12
    KuhnGame game;
    VanillaCfr solver(game);
    solver.run_iterations(100000);
    CHECK(expected_value(game, solver.average_strategy(), 0) == Catch::Approx(-1.0 / 18.0).margin(1e-3));
}

TEST_CASE("kuhn: deterministic — two runs identical") {             // V14
    KuhnGame game;

    VanillaCfr solver_a(game);
    solver_a.run_iterations(10000);

    VanillaCfr solver_b(game);
    solver_b.run_iterations(10000);

    REQUIRE(solver_a.average_strategy() == solver_b.average_strategy());
}

TEST_CASE("kuhn: analytic structure of equilibrium") {              // V15, CHECK-tier
    KuhnGame game;
    VanillaCfr solver(game);
    solver.run_iterations(100000);
    StrategyProfile average = solver.average_strategy();

    // Infoset keys follow KuhnGame::infoset_key's "P<player>:<card>:<betting
    // actions>," convention; action order at each key follows legal_actions
    // (check/bet or call/fold).
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

TEST_CASE("kuhn: exploitability decreases across decade checkpoints") {  // V11, soft convergence check
    // Vanilla CFR is not strictly monotone iteration-to-iteration; compare
    // decade checkpoints only, as a diagnostic that the solver is actually
    // converging rather than stalled or diverging.
    KuhnGame game;
    VanillaCfr solver(game);

    std::vector<int> checkpoint_iterations = {100, 1000, 10000, 100000};
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
