// Best response on Kuhn beats the strategies it is compared with.

#include <catch2/catch_test_macros.hpp>

#include <cstddef>
#include <vector>

#include "oracles/best_response.hpp"
#include "oracles/kuhn.hpp"
#include "tree_walk.hpp"

using namespace cfr::game;
using namespace cfr::solver;

namespace {

template <typename GameT>
StrategyProfile uniform_profile(const GameT& game) {
    StrategyProfile profile;
    walk(game, game.initial_state(), [&](const State& state) {
        if (game.is_terminal(state) || game.is_chance(state)) return;
        InfosetLabel label = game.infoset_label(state);
        if (profile.count(label)) return;
        std::size_t action_count = game.legal_actions(state).size();
        profile[label] = std::vector<double>(
            action_count, 1.0 / static_cast<double>(action_count));
    });
    return profile;
}

template <typename GameT>
StrategyProfile pure_action_profile(const GameT& game, Player player,
                                    bool use_last_action,
                                    StrategyProfile profile) {
    walk(game, game.initial_state(), [&](const State& state) {
        if (game.is_terminal(state) || game.is_chance(state)) return;
        if (game.current_player(state) != player) return;
        auto actions = game.legal_actions(state);
        std::vector<double> strategy(actions.size(), 0.0);
        strategy[use_last_action ? actions.size() - 1 : 0] = 1.0;
        profile[game.infoset_label(state)] = strategy;
    });
    return profile;
}

}

TEST_CASE(
    "exploitability of uniform-random Kuhn profile is large and positive") {
    KuhnGame game;
    StrategyProfile uniform = uniform_profile(game);
    CHECK(exploitability(game, uniform) > 0.1);
}

TEST_CASE("BR value weakly improves on any fixed alternative strategy") {
    KuhnGame game;
    StrategyProfile opponent = uniform_profile(game);
    double br_value = best_response_value(game, opponent, 0);

    bool any_strict_improvement = false;

    for (bool use_last_action : {false, true}) {
        StrategyProfile alternative =
            pure_action_profile(game, 0, use_last_action, opponent);
        double alternative_value = expected_value(game, alternative, 0);
        CHECK(br_value >= alternative_value - 1e-9);
        if (br_value > alternative_value + 1e-9) any_strict_improvement = true;
    }

    double uniform_value = expected_value(game, opponent, 0);
    CHECK(br_value >= uniform_value - 1e-9);
    if (br_value > uniform_value + 1e-9) any_strict_improvement = true;

    CHECK(any_strict_improvement);
}
