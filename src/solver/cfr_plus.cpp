#include "solver/cfr_plus.hpp"

#include <cstddef>
#include <numeric>

namespace cfr::solver {

namespace {

// Strategy-sum entries below this total are treated as "no data yet" and
// fall back to uniform rather than dividing by (near-)zero -- mirrors
// regret_matching_strategy's own all-non-positive fallback (identical
// constant to VanillaCfr's, copied rather than shared per this codebase's
// no-cross-solver-coupling pattern).
constexpr double kStrategySumEpsilon = 1e-12;

}  // namespace

CfrPlus::CfrPlus(const game::Game& game) : game_(game) {}

std::array<double, 2> CfrPlus::traverse(const game::State& state, double player0_reach, double player1_reach,
                                         double chance_reach, game::Player updating_player) {
    if (game_.is_terminal(state)) {
        return {game_.terminal_utility(state, 0), game_.terminal_utility(state, 1)};
    }

    if (game_.is_chance(state)) {
        std::array<double, 2> node_value = {0.0, 0.0};
        for (auto& [action, probability] : game_.chance_outcomes(state)) {
            std::array<double, 2> child_value = traverse(game_.apply_action(state, action), player0_reach,
                                                           player1_reach, chance_reach * probability, updating_player);
            node_value[0] += probability * child_value[0];
            node_value[1] += probability * child_value[1];
        }
        return node_value;
    }

    game::Player acting_player = game_.current_player(state);
    game::InfoSetKey key = game_.infoset_key(state);
    std::vector<game::Action> actions = game_.legal_actions(state);

    // sigma^t for this infoset: regret-matched against the iteration-start
    // snapshot, not the live (possibly already-updated-this-iteration)
    // table -- see class docblock. A key missing from the snapshot
    // (first-ever visit to this infoset) gets the all-zero regret vector,
    // which regret_matching_strategy resolves to uniform, same as the live
    // table's own missing-key behavior. Both players read from the shared
    // snapshot regardless of who updates this iteration -- the
    // non-updating player's live table is untouched this iteration anyway.
    auto snapshot_entry = regret_snapshot_.find(key);
    std::vector<double> strategy = snapshot_entry != regret_snapshot_.end()
                                        ? regret_matching_strategy(snapshot_entry->second)
                                        : regret_matching_strategy(std::vector<double>(actions.size(), 0.0));

    double acting_player_reach = acting_player == 0 ? player0_reach : player1_reach;

    std::array<double, 2> node_value = {0.0, 0.0};
    std::vector<double> acting_player_action_value(actions.size());
    for (std::size_t i = 0; i < actions.size(); ++i) {
        double next_player0_reach = acting_player == 0 ? player0_reach * strategy[i] : player0_reach;
        double next_player1_reach = acting_player == 1 ? player1_reach * strategy[i] : player1_reach;
        std::array<double, 2> child_value = traverse(game_.apply_action(state, actions[i]), next_player0_reach,
                                                       next_player1_reach, chance_reach, updating_player);
        node_value[0] += strategy[i] * child_value[0];
        node_value[1] += strategy[i] * child_value[1];
        acting_player_action_value[i] = child_value[acting_player];
    }

    // Regret and strategy-sum writes only happen on updating_player's own
    // infosets -- alternating updates, CFR+'s other departure from vanilla
    // (which writes both players' every traversal).
    if (acting_player == updating_player) {
        double opponent_reach = acting_player == 0 ? player1_reach : player0_reach;
        double counterfactual_reach = opponent_reach * chance_reach;
        std::vector<double> increment(actions.size());
        for (std::size_t i = 0; i < actions.size(); ++i) {
            increment[i] = counterfactual_reach * (acting_player_action_value[i] - node_value[acting_player]);
        }
        // Floor-write to the live table, never the snapshot -- the
        // snapshot stays frozen at its iteration-start values for the
        // rest of this traversal (see class docblock). Adds to whatever
        // the live table already holds from earlier visits in this same
        // traversal, then floors at 0 -- regret-matching+'s defining
        // trait vs vanilla's unfloored accumulation.
        auto& cumulative_regrets = cumulative_regrets_.try_emplace(key, actions.size(), 0.0).first->second;
        accumulate_regret_plus(cumulative_regrets, increment);

        // Linear averaging: each iteration's contribution to the strategy
        // sum is weighted by iteration_ itself, not uniformly -- CFR+'s
        // other departure from vanilla (which weights every iteration the
        // same).
        auto& strategy_sum = strategy_sums_.try_emplace(key, actions.size(), 0.0).first->second;
        for (std::size_t i = 0; i < actions.size(); ++i) {
            strategy_sum[i] += iteration_ * acting_player_reach * strategy[i];
        }
    }

    return node_value;
}

void CfrPlus::run_iterations(int iteration_count) {
    for (int i = 0; i < iteration_count; ++i) {
        ++iteration_;
        game::Player updating_player = iteration_ % 2;
        regret_snapshot_ = cumulative_regrets_;
        traverse(game_.initial_state(), 1.0, 1.0, 1.0, updating_player);
    }
}

StrategyProfile CfrPlus::current_strategy() const {
    StrategyProfile profile;
    for (const auto& [key, cumulative_regrets] : cumulative_regrets_) {
        profile[key] = regret_matching_strategy(cumulative_regrets);
    }
    return profile;
}

StrategyProfile CfrPlus::average_strategy() const {
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

int CfrPlus::iterations_run() const { return iteration_; }

}  // namespace cfr::solver
