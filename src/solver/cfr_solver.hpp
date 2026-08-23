#pragma once

#include <array>
#include <cassert>
#include <cstddef>
#include <cstdint>
#include <span>
#include <vector>

#include "game/game.hpp"
#include "solver/strategy.hpp"

namespace cfr::solver {

namespace detail {

inline constexpr std::size_t kMaxActionsPerInfoset = 64;

template <typename GameT, typename StrategyFromStored>
requires game::LabelledGame<GameT>
void collect_strategy_profile(const GameT& game, const game::State& state,
                               const std::vector<double>& stored_table, std::size_t stride,
                               StrategyFromStored&& strategy_from_stored, StrategyProfile& profile) {
    if (game.is_terminal(state)) return;

    if (game.is_chance(state)) {
        for (auto& [action, probability] : game.chance_outcomes(state)) {
            collect_strategy_profile(game, game.apply_action(state, action), stored_table, stride,
                                      strategy_from_stored, profile);
        }
        return;
    }

    std::vector<game::Action> actions = game.legal_actions(state);
    std::span<const double> stored(stored_table.data() + game.infoset_index(state) * stride, actions.size());
    profile[game.infoset_label(state)] = strategy_from_stored(stored);

    for (game::Action action : actions) {
        collect_strategy_profile(game, game.apply_action(state, action), stored_table, stride, strategy_from_stored,
                                  profile);
    }
}

}

struct VanillaRules {
    static constexpr const char* kPolicyName = "vanilla";

    static bool updates(int, game::Player) { return true; }

    static void accumulate(std::span<double> cumulative_regrets, std::span<const double> increments) {
        for (std::size_t i = 0; i < cumulative_regrets.size(); ++i) {
            cumulative_regrets[i] += increments[i];
        }
    }

    static double weight(int) { return 1.0; }
};

struct CfrPlusRules {
    static constexpr const char* kPolicyName = "cfr_plus";

    static bool updates(int iteration, game::Player acting_player) { return acting_player == iteration % 2; }

    static void accumulate(std::span<double> cumulative_regrets, std::span<const double> increments) {
        accumulate_regret_plus(cumulative_regrets, increments);
    }

    static double weight(int iteration) { return static_cast<double>(iteration); }
};

template <typename Rules, typename GameT>
requires game::GameLike<GameT>
class CfrSolver {
public:
    explicit CfrSolver(const GameT& game);

    void run_iterations(int iteration_count);

    StrategyProfile average_strategy() const;

    int iterations_run() const;

private:

    std::array<double, 2> traverse(const game::State& state, double player0_reach, double player1_reach,
                                    double chance_reach);

    static constexpr std::size_t kStride = GameT::kMaxActions;

    const GameT& game_;
    std::vector<double> cumulative_regrets_;
    std::vector<double> regret_snapshot_;
    std::vector<double> strategy_sums_;
    int iteration_ = 0;
};

template <typename Rules, typename GameT>
requires game::GameLike<GameT>
CfrSolver<Rules, GameT>::CfrSolver(const GameT& game)
    : game_(game),
      cumulative_regrets_(static_cast<std::size_t>(game.infoset_count()) * kStride, 0.0),
      regret_snapshot_(static_cast<std::size_t>(game.infoset_count()) * kStride, 0.0),
      strategy_sums_(static_cast<std::size_t>(game.infoset_count()) * kStride, 0.0) {}

template <typename Rules, typename GameT>
requires game::GameLike<GameT>
std::array<double, 2> CfrSolver<Rules, GameT>::traverse(const game::State& state, double player0_reach,
                                                          double player1_reach, double chance_reach) {
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
    assert(actions.size() <= detail::kMaxActionsPerInfoset);

    std::size_t table_offset = static_cast<std::size_t>(infoset_index) * kStride;
    std::array<double, detail::kMaxActionsPerInfoset> strategy{};
    regret_matching_strategy_into(std::span<const double>(regret_snapshot_.data() + table_offset, actions.size()),
                                   std::span<double>(strategy.data(), actions.size()));

    double acting_player_reach = acting_player == 0 ? player0_reach : player1_reach;

    std::array<double, 2> node_value = {0.0, 0.0};
    std::array<double, detail::kMaxActionsPerInfoset> acting_player_action_value{};
    for (std::size_t i = 0; i < actions.size(); ++i) {
        double next_player0_reach = acting_player == 0 ? player0_reach * strategy[i] : player0_reach;
        double next_player1_reach = acting_player == 1 ? player1_reach * strategy[i] : player1_reach;
        std::array<double, 2> child_value =
            traverse(game_.apply_action(state, actions[i]), next_player0_reach, next_player1_reach, chance_reach);
        node_value[0] += strategy[i] * child_value[0];
        node_value[1] += strategy[i] * child_value[1];
        acting_player_action_value[i] = child_value[acting_player];
    }

    if (Rules::updates(iteration_, acting_player)) {
        double opponent_reach = acting_player == 0 ? player1_reach : player0_reach;
        double counterfactual_reach = opponent_reach * chance_reach;
        std::array<double, detail::kMaxActionsPerInfoset> increment{};
        for (std::size_t i = 0; i < actions.size(); ++i) {
            increment[i] = counterfactual_reach * (acting_player_action_value[i] - node_value[acting_player]);
        }

        Rules::accumulate(std::span<double>(cumulative_regrets_.data() + table_offset, actions.size()),
                          std::span<const double>(increment.data(), actions.size()));

        std::span<double> strategy_sum(strategy_sums_.data() + table_offset, actions.size());
        double strategy_weight = Rules::weight(iteration_);
        for (std::size_t i = 0; i < actions.size(); ++i) {
            strategy_sum[i] += strategy_weight * acting_player_reach * strategy[i];
        }
    }

    return node_value;
}

template <typename Rules, typename GameT>
requires game::GameLike<GameT>
void CfrSolver<Rules, GameT>::run_iterations(int iteration_count) {
    for (int i = 0; i < iteration_count; ++i) {
        ++iteration_;
        regret_snapshot_ = cumulative_regrets_;
        traverse(game_.initial_state(), 1.0, 1.0, 1.0);
    }
}

template <typename Rules, typename GameT>
requires game::GameLike<GameT>
StrategyProfile CfrSolver<Rules, GameT>::average_strategy() const {
    StrategyProfile profile;
    detail::collect_strategy_profile(game_, game_.initial_state(), strategy_sums_, kStride, average_from_sums,
                                      profile);
    return profile;
}

template <typename Rules, typename GameT>
requires game::GameLike<GameT>
int CfrSolver<Rules, GameT>::iterations_run() const {
    return iteration_;
}

template <typename GameT> using VanillaCfr = CfrSolver<VanillaRules, GameT>;
template <typename GameT> using CfrPlus = CfrSolver<CfrPlusRules, GameT>;

}
