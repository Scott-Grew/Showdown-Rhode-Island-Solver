// Exact best response and exploitability by a walk of the full tree,
// keyed by infoset label. For Kuhn and Leduc; Rhode Island uses
// rih_best_response.hpp.

#pragma once

#include <algorithm>
#include <cassert>
#include <cstddef>
#include <map>
#include <utility>
#include <vector>

#include "game/game.hpp"
#include "solver/strategy.hpp"

namespace cfr::solver {

namespace detail {

// A responder state with the probability that chance and the
// opponent lead to it.
struct WeightedState {
    game::State state;
    double reach_weight;
};

// Every responder state of each infoset.
using StatesByInfoset =
    std::map<game::InfosetLabel, std::vector<WeightedState>>;

// Best-response value already computed per infoset.
using BestResponseMemo = std::map<game::InfosetLabel, double>;

// The opponent's strategy at state, or uniform when the profile
// has no entry for that infoset.
template <typename GameT>
std::vector<double> opponent_action_probabilities(
    const GameT& game, const game::State& state,
    const StrategyProfile& opponent_strategy,
    const std::vector<game::Action>& actions) {
    auto profile_entry = opponent_strategy.find(game.infoset_label(state));

    if (profile_entry != opponent_strategy.end()) {
        assert(profile_entry->second.size() == actions.size());
        return profile_entry->second;
    }
    return std::vector<double>(actions.size(),
                               1.0 / static_cast<double>(actions.size()));
}

// Infosets with total reach at or below this get value zero.
constexpr double kUnreachableReachWeight = 1e-12;

// Groups the responder's decision states by infoset. reach_weight
// multiplies chance and opponent probabilities only.
template <typename GameT>
void collect_responder_states(const GameT& game, const game::State& state,
                              double reach_weight,
                              const StrategyProfile& opponent_strategy,
                              game::Player responder,
                              StatesByInfoset& states_by_infoset) {
    if (game.is_terminal(state)) return;

    if (game.is_chance(state)) {
        for (auto& [action, probability] : game.chance_outcomes(state)) {
            collect_responder_states(game, game.apply_action(state, action),
                                     reach_weight * probability,
                                     opponent_strategy, responder,
                                     states_by_infoset);
        }
        return;
    }

    std::vector<game::Action> actions = game.legal_actions(state);

    if (game.current_player(state) == responder) {
        states_by_infoset[game.infoset_label(state)].push_back(
            {state, reach_weight});
        for (game::Action action : actions) {
            collect_responder_states(game, game.apply_action(state, action),
                                     reach_weight, opponent_strategy, responder,
                                     states_by_infoset);
        }
        return;
    }

    std::vector<double> action_probabilities =
        opponent_action_probabilities(game, state, opponent_strategy, actions);
    for (std::size_t i = 0; i < actions.size(); ++i) {
        collect_responder_states(game, game.apply_action(state, actions[i]),
                                 reach_weight * action_probabilities[i],
                                 opponent_strategy, responder,
                                 states_by_infoset);
    }
}

// Declared early because it and
// best_response_value_at call each other.
template <typename GameT>
double node_value(const GameT& game, const game::State& state,
                  const StrategyProfile& opponent_strategy,
                  game::Player responder,
                  const StatesByInfoset& states_by_infoset,
                  BestResponseMemo& memo);

// Value per unit of reach of the best single action at an infoset,
// chosen jointly for all states the responder cannot tell apart.
template <typename GameT>
double best_response_value_at(const GameT& game,
                              const game::InfosetLabel& infoset_label,
                              const StrategyProfile& opponent_strategy,
                              game::Player responder,
                              const StatesByInfoset& states_by_infoset,
                              BestResponseMemo& memo) {
    auto memo_entry = memo.find(infoset_label);
    if (memo_entry != memo.end()) return memo_entry->second;

    const std::vector<WeightedState>& weighted_states =
        states_by_infoset.at(infoset_label);
    std::vector<game::Action> actions =
        game.legal_actions(weighted_states.front().state);
    std::vector<double> action_totals(actions.size(), 0.0);
    double total_reach_weight = 0.0;

    for (auto& [state, reach_weight] : weighted_states) {
        total_reach_weight += reach_weight;
        for (std::size_t i = 0; i < actions.size(); ++i) {
            action_totals[i] +=
                reach_weight * node_value(game,
                                          game.apply_action(state, actions[i]),
                                          opponent_strategy, responder,
                                          states_by_infoset, memo);
        }
    }

    double best_value =
        *std::max_element(action_totals.begin(), action_totals.end());
    double normalized_best_value = total_reach_weight > kUnreachableReachWeight
                                       ? best_value / total_reach_weight
                                       : 0.0;
    memo[infoset_label] = normalized_best_value;
    return normalized_best_value;
}

// Expected responder value below state when the responder plays a
// best response and the opponent follows the profile.
template <typename GameT>
double node_value(const GameT& game, const game::State& state,
                  const StrategyProfile& opponent_strategy,
                  game::Player responder,
                  const StatesByInfoset& states_by_infoset,
                  BestResponseMemo& memo) {
    if (game.is_terminal(state)) {
        return game.terminal_utility(state, responder);
    }

    if (game.is_chance(state)) {
        double expected_value = 0.0;
        for (auto& [action, probability] : game.chance_outcomes(state)) {
            expected_value +=
                probability * node_value(game, game.apply_action(state, action),
                                         opponent_strategy, responder,
                                         states_by_infoset, memo);
        }
        return expected_value;
    }

    if (game.current_player(state) == responder) {
        return best_response_value_at(game, game.infoset_label(state),
                                      opponent_strategy, responder,
                                      states_by_infoset, memo);
    }

    std::vector<game::Action> actions = game.legal_actions(state);
    std::vector<double> action_probabilities =
        opponent_action_probabilities(game, state, opponent_strategy, actions);

    double expected_value = 0.0;
    for (std::size_t i = 0; i < actions.size(); ++i) {
        expected_value +=
            action_probabilities[i] *
            node_value(game, game.apply_action(state, actions[i]),
                       opponent_strategy, responder, states_by_infoset, memo);
    }
    return expected_value;
}

}

// Expected chips the responder wins per hand with a best response
// to opponent_strategy. Walks the full tree, so small games only.
template <typename GameT>
    requires game::LabelledGame<GameT>
double best_response_value(const GameT& game,
                           const StrategyProfile& opponent_strategy,
                           game::Player responder) {
    detail::StatesByInfoset states_by_infoset;
    detail::collect_responder_states(game, game.initial_state(), 1.0,
                                     opponent_strategy, responder,
                                     states_by_infoset);

    detail::BestResponseMemo memo;
    return detail::node_value(game, game.initial_state(), opponent_strategy,
                              responder, states_by_infoset, memo);
}

// Mean of the two best-response values; zero exactly at a Nash
// equilibrium of a zero-sum game.
template <typename GameT>
    requires game::LabelledGame<GameT>
double exploitability(const GameT& game, const StrategyProfile& profile) {
    return (best_response_value(game, profile, 0) +
            best_response_value(game, profile, 1)) /
           2.0;
}

}
