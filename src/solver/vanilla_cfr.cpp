#include "solver/vanilla_cfr.hpp"

#include <cstddef>
#include <numeric>

namespace cfr::solver {

namespace {

constexpr double kStrategySumEpsilon = 1e-12;

}

VanillaCfr::VanillaCfr(const game::Game& game) : game_(game) {}

std::array<double, 2> VanillaCfr::traverse(const game::State& state, double player0_reach, double player1_reach,
                                            double chance_reach) {
    if (game_.is_terminal(state)) {
        return {game_.terminal_utility(state, 0), game_.terminal_utility(state, 1)};
    }

    if (game_.is_chance(state)) {
        std::array<double, 2> node_value = {0.0, 0.0};
        for (auto& [action, probability] : game_.chance_outcomes(state)) {
            std::array<double, 2> child_value =
                traverse(game_.apply_action(state, action), player0_reach, player1_reach, chance_reach * probability);
            node_value[0] += probability * child_value[0];
            node_value[1] += probability * child_value[1];
        }
        return node_value;
    }

    game::Player acting_player = game_.current_player(state);
    game::InfoSetKey key = game_.infoset_key(state);
    std::vector<game::Action> actions = game_.legal_actions(state);

    auto snapshot_entry = regret_snapshot_.find(key);
    std::vector<double> strategy = snapshot_entry != regret_snapshot_.end()
                                        ? regret_matching_strategy(snapshot_entry->second)
                                        : regret_matching_strategy(std::vector<double>(actions.size(), 0.0));

    double acting_player_reach = acting_player == 0 ? player0_reach : player1_reach;
    auto& strategy_sum = strategy_sums_.try_emplace(key, actions.size(), 0.0).first->second;
    for (std::size_t i = 0; i < actions.size(); ++i) {
        strategy_sum[i] += acting_player_reach * strategy[i];
    }

    std::array<double, 2> node_value = {0.0, 0.0};
    std::vector<double> acting_player_action_value(actions.size());
    for (std::size_t i = 0; i < actions.size(); ++i) {
        double next_player0_reach = acting_player == 0 ? player0_reach * strategy[i] : player0_reach;
        double next_player1_reach = acting_player == 1 ? player1_reach * strategy[i] : player1_reach;
        std::array<double, 2> child_value =
            traverse(game_.apply_action(state, actions[i]), next_player0_reach, next_player1_reach, chance_reach);
        node_value[0] += strategy[i] * child_value[0];
        node_value[1] += strategy[i] * child_value[1];
        acting_player_action_value[i] = child_value[acting_player];
    }

    auto& cumulative_regrets = cumulative_regrets_.try_emplace(key, actions.size(), 0.0).first->second;
    double opponent_reach = acting_player == 0 ? player1_reach : player0_reach;
    double counterfactual_reach = opponent_reach * chance_reach;
    for (std::size_t i = 0; i < actions.size(); ++i) {
        double regret = acting_player_action_value[i] - node_value[acting_player];
        cumulative_regrets[i] += counterfactual_reach * regret;
    }

    return node_value;
}

void VanillaCfr::run_iterations(int iteration_count) {
    for (int i = 0; i < iteration_count; ++i) {
        regret_snapshot_ = cumulative_regrets_;
        traverse(game_.initial_state(), 1.0, 1.0, 1.0);
    }
}

StrategyProfile VanillaCfr::current_strategy() const {
    StrategyProfile profile;
    for (const auto& [key, cumulative_regrets] : cumulative_regrets_) {
        profile[key] = regret_matching_strategy(cumulative_regrets);
    }
    return profile;
}

StrategyProfile VanillaCfr::average_strategy() const {
    StrategyProfile profile;
    for (const auto& [key, strategy_sum] : strategy_sums_) {
        double total = std::accumulate(strategy_sum.begin(), strategy_sum.end(), 0.0);

        std::vector<double> strategy(strategy_sum.size());
        if (total > kStrategySumEpsilon) {
            for (std::size_t i = 0; i < strategy_sum.size(); ++i) {
                strategy[i] = strategy_sum[i] / total;
            }
        } else {
            double uniform_probability = 1.0 / static_cast<double>(strategy_sum.size());
            for (double& probability : strategy) {
                probability = uniform_probability;
            }
        }
        profile[key] = strategy;
    }
    return profile;
}

}
