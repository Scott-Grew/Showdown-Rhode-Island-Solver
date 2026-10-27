// Test oracle: full-tree vanilla CFR for Kuhn and Leduc.

#pragma once

#include <array>
#include <cassert>
#include <cstddef>
#include <span>
#include <vector>

#include "game/game.hpp"
#include "oracles/labels.hpp"
#include "solver/strategy.hpp"

namespace cfr::solver {

namespace detail {

// Copies one stored table over the reachable tree into a label-keyed profile.
template <typename GameT, typename StrategyFromStored>
    requires game::LabelledGame<GameT>
void collect_strategy_profile(const GameT& game, const game::State& state,
                              const std::vector<double>& stored_table,
                              std::size_t stride,
                              StrategyFromStored&& strategy_from_stored,
                              StrategyProfile& profile) {
    if (game.is_terminal(state)) return;

    if (game.is_chance(state)) {
        for (auto& [action, probability] : game.chance_outcomes(state)) {
            collect_strategy_profile(game, game.apply_action(state, action),
                                     stored_table, stride, strategy_from_stored,
                                     profile);
        }
        return;
    }

    auto actions = game.legal_actions(state);
    std::span<const double> stored(
        stored_table.data() + game.infoset_index(state) * stride,
        actions.size());
    profile[game.infoset_label(state)] = strategy_from_stored(stored);

    for (game::Action action : actions) {
        collect_strategy_profile(game, game.apply_action(state, action),
                                 stored_table, stride, strategy_from_stored,
                                 profile);
    }
}

}

// Full-tree vanilla CFR. kStride slots per infoset. The game is held
// by reference and must outlive the solver. A pass reads regrets from
// the snapshot taken at its start, so its own updates cannot change it.
template <typename GameT>
    requires game::GameLike<GameT>
class CfrSolver {
public:
    explicit CfrSolver(const GameT& game)
        : game_(game),
          cumulative_regrets_(
              static_cast<std::size_t>(game.infoset_count()) * kStride, 0.0),
          regret_snapshot_(
              static_cast<std::size_t>(game.infoset_count()) * kStride, 0.0),
          strategy_sums_(
              static_cast<std::size_t>(game.infoset_count()) * kStride, 0.0) {}

    void run_iterations(int iteration_count) {
        for (int i = 0; i < iteration_count; ++i) {
            regret_snapshot_ = cumulative_regrets_;
            traverse(game_.initial_state(), 1.0, 1.0, 1.0);
        }
    }

    StrategyProfile average_strategy() const {
        StrategyProfile profile;
        detail::collect_strategy_profile(
            game_, game_.initial_state(), strategy_sums_, kStride,
            [](std::span<const double> sums) {
                std::vector<double> strategy(sums.begin(), sums.end());
                normalize_or_uniform(strategy);
                return strategy;
            },
            profile);
        return profile;
    }

private:
    static constexpr std::size_t kStride = GameT::kMaxActions;

    std::array<double, 2> traverse(const game::State& state,
                                   double player0_reach, double player1_reach,
                                   double chance_reach) {
        if (game_.is_terminal(state)) {
            return {game_.terminal_utility(state, 0),
                    game_.terminal_utility(state, 1)};
        }
        if (game_.is_chance(state)) {
            return traverse_chance(state, player0_reach, player1_reach,
                                   chance_reach);
        }
        return traverse_player(state, player0_reach, player1_reach,
                               chance_reach);
    }

    std::array<double, 2> traverse_chance(const game::State& state,
                                          double player0_reach,
                                          double player1_reach,
                                          double chance_reach) {
        std::array<double, 2> node_value = {0.0, 0.0};
        for (auto& [action, probability] : game_.chance_outcomes(state)) {
            std::array<double, 2> child_value =
                traverse(game_.apply_action(state, action), player0_reach,
                         player1_reach, chance_reach * probability);
            node_value[0] += probability * child_value[0];
            node_value[1] += probability * child_value[1];
        }
        return node_value;
    }

    std::array<double, 2> traverse_player(const game::State& state,
                                          double player0_reach,
                                          double player1_reach,
                                          double chance_reach);

    const GameT& game_;
    std::vector<double> cumulative_regrets_;
    std::vector<double> regret_snapshot_;
    std::vector<double> strategy_sums_;
};

template <typename GameT>
    requires game::GameLike<GameT>
std::array<double, 2> CfrSolver<GameT>::traverse_player(
    const game::State& state, double player0_reach, double player1_reach,
    double chance_reach) {
    game::Player acting_player = game_.current_player(state);
    auto actions = game_.legal_actions(state);
    assert(actions.size() <= detail::kMaxActionsPerInfoset);

    std::size_t table_offset =
        static_cast<std::size_t>(game_.infoset_index(state)) * kStride;
    std::array<double, detail::kMaxActionsPerInfoset> strategy{};
    std::span<double> matched(strategy.data(), actions.size());
    std::span<const double> regrets(regret_snapshot_.data() + table_offset,
                                    actions.size());
    for (std::size_t i = 0; i < actions.size(); ++i) matched[i] = regrets[i];
    regret_matching_in_place(matched);

    double acting_player_reach =
        acting_player == 0 ? player0_reach : player1_reach;
    std::array<double, 2> node_value = {0.0, 0.0};
    std::array<double, detail::kMaxActionsPerInfoset>
        acting_player_action_value{};
    for (std::size_t i = 0; i < actions.size(); ++i) {
        double next_player0_reach =
            acting_player == 0 ? player0_reach * strategy[i] : player0_reach;
        double next_player1_reach =
            acting_player == 1 ? player1_reach * strategy[i] : player1_reach;
        std::array<double, 2> child_value =
            traverse(game_.apply_action(state, actions[i]), next_player0_reach,
                     next_player1_reach, chance_reach);
        node_value[0] += strategy[i] * child_value[0];
        node_value[1] += strategy[i] * child_value[1];
        acting_player_action_value[i] = child_value[acting_player];
    }

    double opponent_reach = acting_player == 0 ? player1_reach : player0_reach;
    // Regret uses every reach except the acting player's. The average
    // strategy is weighted by the acting player's own reach.
    double counterfactual_reach = opponent_reach * chance_reach;
    for (std::size_t i = 0; i < actions.size(); ++i) {
        cumulative_regrets_[table_offset + i] +=
            counterfactual_reach *
            (acting_player_action_value[i] - node_value[acting_player]);
        strategy_sums_[table_offset + i] += acting_player_reach * strategy[i];
    }
    return node_value;
}

}
