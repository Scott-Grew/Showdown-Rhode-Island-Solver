// Tree walk and the checks shared by the Kuhn, Leduc and Rhode Island suites.

#pragma once

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include <array>
#include <cstdint>
#include <map>
#include <set>
#include <type_traits>
#include <utility>

#include "game/betting_round.hpp"
#include "game/game.hpp"
#include "oracles/labels.hpp"

namespace cfr::game {

// Solvers copy State by value on every node. A non-trivial member
// would turn those copies into constructor calls.
static_assert(std::is_trivially_copyable_v<State>);
static_assert(std::is_trivially_destructible_v<State>);

template <typename GameT, typename Fn>
void walk(const GameT& game, const State& state, Fn&& visit) {
    visit(state);
    if (game.is_terminal(state)) return;
    if (game.is_chance(state)) {
        for (auto& [action, probability] : game.chance_outcomes(state))
            walk(game, game.apply_action(state, action), visit);
        return;
    }
    for (Action action : game.legal_actions(state))
        walk(game, game.apply_action(state, action), visit);
}

// Chance probabilities are a distribution. The sum is not implied by
// listing one outcome per remaining card unless each weight is 1/n.
template <typename GameT>
void require_chance_distribution(const GameT& game) {
    walk(game, game.initial_state(), [&](const State& state) {
        if (!game.is_chance(state)) return;
        double sum = 0.0;
        for (auto& [action, probability] : game.chance_outcomes(state))
            sum += probability;
        REQUIRE_THAT(sum, Catch::Matchers::WithinAbs(1.0, 1e-12));
    });
}

// An empty action list is a decision the solvers never enter.
template <typename GameT>
void require_decisions_actionable(const GameT& game, const State& root) {
    walk(game, root, [&](const State& state) {
        if (game.is_terminal(state) || game.is_chance(state)) return;
        REQUIRE_FALSE(game.legal_actions(state).empty());
    });
}

// Label and index are one bijection onto [0, infoset_count). Returns
// the row count so the caller can pin a known integer.
template <typename GameT>
std::size_t require_infoset_bijection(const GameT& game) {
    std::map<InfosetLabel, std::uint32_t> index_by_label;
    std::set<std::uint32_t> distinct_indices;
    walk(game, game.initial_state(), [&](const State& state) {
        if (game.is_terminal(state) || game.is_chance(state)) return;
        InfosetLabel label = game.infoset_label(state);
        std::uint32_t index = game.infoset_index(state);
        REQUIRE(index < game.infoset_count());
        auto existing = index_by_label.emplace(label, index).first;
        REQUIRE(existing->second == index);
        distinct_indices.insert(index);
    });
    REQUIRE(index_by_label.size() == distinct_indices.size());
    REQUIRE(distinct_indices.size() == game.infoset_count());
    return distinct_indices.size();
}

// Both players have matched when a round closes without a fold, so
// the payoff is half the pot or a tie. contribution is the chip
// vector, read separately from terminal_utility.
template <typename GameT, typename Contribution>
void require_showdown_pot(const GameT& game, const State& root,
                          Contribution contribution) {
    long long showdowns = 0;
    walk(game, root, [&](const State& state) {
        if (!game.is_terminal(state)) return;
        if (state.history[state.history_len - 1] == kActionFold) return;
        ++showdowns;
        std::array<int, 2> chips = contribution(state);
        REQUIRE(chips[0] == chips[1]);
        int pot = chips[0] + chips[1];
        REQUIRE(pot % 2 == 0);
        double utility = game.terminal_utility(state, 0);
        double half_pot = pot / 2.0;
        REQUIRE(
            (utility == 0.0 || utility == half_pot || utility == -half_pot));
    });
    REQUIRE(showdowns > 0);
}

}

namespace cfr::solver {

template <typename GameT>
double expected_value(const GameT& game, const StrategyProfile& profile,
                      game::Player player, const game::State& state) {
    if (game.is_terminal(state)) return game.terminal_utility(state, player);

    if (game.is_chance(state)) {
        double value = 0.0;
        for (auto& [action, probability] : game.chance_outcomes(state))
            value +=
                probability * expected_value(game, profile, player,
                                             game.apply_action(state, action));
        return value;
    }

    auto actions = game.legal_actions(state);
    auto profile_entry = profile.find(game.infoset_label(state));
    double uniform_probability = 1.0 / static_cast<double>(actions.size());
    double value = 0.0;
    for (std::size_t i = 0; i < actions.size(); ++i) {
        double action_probability = profile_entry != profile.end()
                                        ? profile_entry->second[i]
                                        : uniform_probability;
        value += action_probability *
                 expected_value(game, profile, player,
                                game.apply_action(state, actions[i]));
    }
    return value;
}

template <typename GameT>
double expected_value(const GameT& game, const StrategyProfile& profile,
                      game::Player player) {
    return expected_value(game, profile, player, game.initial_state());
}

}
