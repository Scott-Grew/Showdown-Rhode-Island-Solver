#include <catch2/catch_test_macros.hpp>

#include <cstddef>
#include <vector>

#include "game/kuhn.hpp"
#include "solver/best_response.hpp"
#include "solver/strategy.hpp"
#include "tree_walk.hpp"

using namespace cfr::game;
using namespace cfr::solver;

namespace {

// Test-only verification instruments (obviously-correct recursive walks,
// deliberately kept out of src/solver -- these exist to check the
// production best-response code against, not to be reused by it).

// Uniform profile: every infoset reachable in the tree gets equal
// probability over its legal actions. Doubles as a fixed baseline opponent
// strategy and as the starting point for the hand-built alternatives below.
StrategyProfile uniform_profile(const Game& game) {
    StrategyProfile profile;
    walk(game, game.initial_state(), [&](const State& state) {
        if (game.is_terminal(state) || game.is_chance(state)) return;
        InfoSetKey key = game.infoset_key(state);
        if (profile.count(key)) return;
        std::size_t action_count = game.legal_actions(state).size();
        profile[key] = std::vector<double>(action_count, 1.0 / static_cast<double>(action_count));
    });
    return profile;
}

// Overwrites `player`'s entries in a copy of `profile` so every one of
// their infosets deterministically plays a single fixed legal action:
// index 0 (first) if use_last_action is false, the last legal action
// otherwise. The other player's entries are left untouched.
StrategyProfile pure_action_profile(const Game& game, Player player, bool use_last_action, StrategyProfile profile) {
    walk(game, game.initial_state(), [&](const State& state) {
        if (game.is_terminal(state) || game.is_chance(state)) return;
        if (game.current_player(state) != player) return;
        std::vector<Action> actions = game.legal_actions(state);
        std::vector<double> strategy(actions.size(), 0.0);
        strategy[use_last_action ? actions.size() - 1 : 0] = 1.0;
        profile[game.infoset_key(state)] = strategy;
    });
    return profile;
}

// Expected value for `player` under a full joint profile -- one map
// covering both players' infosets, exactly like best_response_value's
// opponent_strategy argument -- via plain policy evaluation (no best
// response). Missing infosets fall back to uniform, mirroring
// best_response_value's documented convention so the two are comparable.
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

TEST_CASE("exploitability of uniform-random Kuhn profile is large and positive") {  // V13
    KuhnGame game;
    StrategyProfile uniform = uniform_profile(game);
    CHECK(exploitability(game, uniform) > 0.1);
}

TEST_CASE("BR value weakly improves on any fixed alternative strategy") {  // V13
    KuhnGame game;
    StrategyProfile opponent = uniform_profile(game);  // fixes both players' baseline strategy
    double br_value = best_response_value(game, opponent, 0);

    bool any_strict_improvement = false;

    for (bool use_last_action : {false, true}) {
        StrategyProfile alternative = pure_action_profile(game, 0, use_last_action, opponent);
        double alternative_value = expected_value(game, alternative, 0);
        CHECK(br_value >= alternative_value - 1e-9);
        if (br_value > alternative_value + 1e-9) any_strict_improvement = true;
    }

    // third hand-built profile: uniform itself (player 0 unchanged from `opponent`)
    double uniform_value = expected_value(game, opponent, 0);
    CHECK(br_value >= uniform_value - 1e-9);
    if (br_value > uniform_value + 1e-9) any_strict_improvement = true;

    CHECK(any_strict_improvement);
}

TEST_CASE("exploitability is symmetric-nonnegative") {  // V13
    KuhnGame game;
    StrategyProfile uniform = uniform_profile(game);
    CHECK(exploitability(game, uniform) >= 0.0);

    StrategyProfile player0_always_first = pure_action_profile(game, 0, false, uniform);
    CHECK(exploitability(game, player0_always_first) >= 0.0);

    StrategyProfile player1_always_last = pure_action_profile(game, 1, true, uniform);
    CHECK(exploitability(game, player1_always_last) >= 0.0);
}
