#include "solver/best_response.hpp"

#include <algorithm>
#include <cassert>
#include <cstddef>
#include <limits>

namespace cfr::solver {

namespace {

using StatesByInfoset = std::map<game::InfoSetKey, std::vector<std::pair<game::State, double>>>;

using BestResponseMemo = std::map<game::InfoSetKey, double>;

std::vector<double> opponent_action_probabilities(const game::Game& game, const game::State& state,
                                                    const StrategyProfile& opponent_strategy,
                                                    const std::vector<game::Action>& actions) {
    auto profile_entry = opponent_strategy.find(game.infoset_key(state));

    if (profile_entry != opponent_strategy.end()) {
        assert(profile_entry->second.size() == actions.size());
        return profile_entry->second;
    }
    return std::vector<double>(actions.size(), 1.0 / static_cast<double>(actions.size()));
}

constexpr double kUnreachableInfosetWeightEpsilon = 1e-12;

void collect_responder_states(const game::Game& game, const game::State& state, double reach_weight,
                               const StrategyProfile& opponent_strategy, game::Player responder,
                               StatesByInfoset& states_by_infoset) {
    if (game.is_terminal(state)) return;

    if (game.is_chance(state)) {
        for (auto& [action, probability] : game.chance_outcomes(state)) {
            collect_responder_states(game, game.apply_action(state, action), reach_weight * probability,
                                      opponent_strategy, responder, states_by_infoset);
        }
        return;
    }

    std::vector<game::Action> actions = game.legal_actions(state);

    if (game.current_player(state) == responder) {
        states_by_infoset[game.infoset_key(state)].emplace_back(state, reach_weight);
        for (game::Action action : actions) {
            collect_responder_states(game, game.apply_action(state, action), reach_weight, opponent_strategy,
                                      responder, states_by_infoset);
        }
        return;
    }

    std::vector<double> action_probabilities = opponent_action_probabilities(game, state, opponent_strategy, actions);
    for (std::size_t i = 0; i < actions.size(); ++i) {
        collect_responder_states(game, game.apply_action(state, actions[i]),
                                  reach_weight * action_probabilities[i], opponent_strategy, responder,
                                  states_by_infoset);
    }
}

double node_value(const game::Game& game, const game::State& state, const StrategyProfile& opponent_strategy,
                   game::Player responder, const StatesByInfoset& states_by_infoset, BestResponseMemo& memo);

double best_response_value_at(const game::Game& game, const game::InfoSetKey& infoset_key,
                               const StrategyProfile& opponent_strategy, game::Player responder,
                               const StatesByInfoset& states_by_infoset, BestResponseMemo& memo) {
    auto memo_entry = memo.find(infoset_key);
    if (memo_entry != memo.end()) return memo_entry->second;

    const std::vector<std::pair<game::State, double>>& weighted_states = states_by_infoset.at(infoset_key);
    std::vector<game::Action> actions = game.legal_actions(weighted_states.front().first);
    std::vector<double> action_totals(actions.size(), 0.0);
    double total_reach_weight = 0.0;

    for (auto& [state, reach_weight] : weighted_states) {
        total_reach_weight += reach_weight;
        for (std::size_t i = 0; i < actions.size(); ++i) {
            action_totals[i] += reach_weight * node_value(game, game.apply_action(state, actions[i]),
                                                            opponent_strategy, responder, states_by_infoset, memo);
        }
    }

    double best_value = *std::max_element(action_totals.begin(), action_totals.end());
    double normalized_best_value =
        total_reach_weight > kUnreachableInfosetWeightEpsilon ? best_value / total_reach_weight : 0.0;
    memo[infoset_key] = normalized_best_value;
    return normalized_best_value;
}

double node_value(const game::Game& game, const game::State& state, const StrategyProfile& opponent_strategy,
                   game::Player responder, const StatesByInfoset& states_by_infoset, BestResponseMemo& memo) {
    if (game.is_terminal(state)) {
        return game.terminal_utility(state, responder);
    }

    if (game.is_chance(state)) {
        double expected_value = 0.0;
        for (auto& [action, probability] : game.chance_outcomes(state)) {
            expected_value += probability * node_value(game, game.apply_action(state, action), opponent_strategy,
                                                         responder, states_by_infoset, memo);
        }
        return expected_value;
    }

    if (game.current_player(state) == responder) {
        return best_response_value_at(game, game.infoset_key(state), opponent_strategy, responder, states_by_infoset,
                                       memo);
    }

    std::vector<game::Action> actions = game.legal_actions(state);
    std::vector<double> action_probabilities = opponent_action_probabilities(game, state, opponent_strategy, actions);

    double expected_value = 0.0;
    for (std::size_t i = 0; i < actions.size(); ++i) {
        expected_value += action_probabilities[i] * node_value(game, game.apply_action(state, actions[i]),
                                                                 opponent_strategy, responder, states_by_infoset,
                                                                 memo);
    }
    return expected_value;
}

}

double best_response_value(const game::Game& game, const StrategyProfile& opponent_strategy,
                            game::Player responder) {
    StatesByInfoset states_by_infoset;
    collect_responder_states(game, game.initial_state(), 1.0, opponent_strategy, responder, states_by_infoset);

    BestResponseMemo memo;
    return node_value(game, game.initial_state(), opponent_strategy, responder, states_by_infoset, memo);
}

double exploitability(const game::Game& game, const StrategyProfile& profile) {
    return (best_response_value(game, profile, 0) + best_response_value(game, profile, 1)) / 2.0;
}

}
