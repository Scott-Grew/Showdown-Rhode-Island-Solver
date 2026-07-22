#include "solver/vanilla_cfr.hpp"

#include <cstddef>
#include <numeric>

namespace cfr::solver {

namespace {

constexpr double kStrategySumEpsilon = 1e-12;

template <typename StrategyFromStored>
void collect_strategy_profile(const game::Game& game, const game::State& state,
                               const std::vector<std::vector<double>>& stored_table,
                               StrategyFromStored&& strategy_from_stored, StrategyProfile& profile) {
    if (game.is_terminal(state)) return;

    if (game.is_chance(state)) {
        for (auto& [action, probability] : game.chance_outcomes(state)) {
            collect_strategy_profile(game, game.apply_action(state, action), stored_table, strategy_from_stored,
                                      profile);
        }
        return;
    }

    const std::vector<double>& stored = stored_table[game.infoset_index(state)];
    if (!stored.empty()) {
        profile[game.infoset_label(state)] = strategy_from_stored(stored);
    }

    for (game::Action action : game.legal_actions(state)) {
        collect_strategy_profile(game, game.apply_action(state, action), stored_table, strategy_from_stored, profile);
    }
}

}

VanillaCfr::VanillaCfr(const game::Game& game)
    : game_(game),
      cumulative_regrets_(game.infoset_count()),
      regret_snapshot_(game.infoset_count()),
      strategy_sums_(game.infoset_count()) {}

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
    std::uint32_t infoset_index = game_.infoset_index(state);
    std::vector<game::Action> actions = game_.legal_actions(state);

    const std::vector<double>& regret_snapshot = regret_snapshot_[infoset_index];
    std::vector<double> strategy = !regret_snapshot.empty()
                                        ? regret_matching_strategy(regret_snapshot)
                                        : regret_matching_strategy(std::vector<double>(actions.size(), 0.0));

    double acting_player_reach = acting_player == 0 ? player0_reach : player1_reach;
    std::vector<double>& strategy_sum = strategy_sums_[infoset_index];
    if (strategy_sum.empty()) strategy_sum.assign(actions.size(), 0.0);
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

    std::vector<double>& cumulative_regrets = cumulative_regrets_[infoset_index];
    if (cumulative_regrets.empty()) cumulative_regrets.assign(actions.size(), 0.0);
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
    collect_strategy_profile(game_, game_.initial_state(), cumulative_regrets_, regret_matching_strategy, profile);
    return profile;
}

StrategyProfile VanillaCfr::average_strategy() const {
    StrategyProfile profile;
    collect_strategy_profile(
        game_, game_.initial_state(), strategy_sums_,
        [](const std::vector<double>& strategy_sum) {
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
            return strategy;
        },
        profile);
    return profile;
}

}
