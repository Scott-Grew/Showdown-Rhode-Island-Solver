// The strategy container and the regret-matching arithmetic that both
// solvers share.

#pragma once

#include <algorithm>
#include <cstddef>
#include <map>
#include <span>
#include <vector>

#include "game/game.hpp"

namespace cfr::solver {

// Action probabilities per infoset label, in legal_actions order.
using StrategyProfile = std::map<game::InfosetLabel, std::vector<double>>;

// Totals at or below this count as empty and become uniform.
inline constexpr double kStrategySumEpsilon = 1e-12;

// Scales values to sum to 1 in place, or sets them uniform when
// the total is not above epsilon.
inline void normalize_or_uniform(std::span<double> values,
                                 double epsilon = kStrategySumEpsilon) {
    double total = 0.0;
    for (double value : values) total += value;

    if (total > epsilon) {
        for (double& value : values) value /= total;
        return;
    }
    double uniform_probability = 1.0 / static_cast<double>(values.size());
    for (double& value : values) value = uniform_probability;
}

// Average strategy of one infoset from its accumulated sums.
inline std::vector<double> average_from_sums(
    std::span<const double> strategy_sum) {
    std::vector<double> strategy(strategy_sum.begin(), strategy_sum.end());
    normalize_or_uniform(strategy);
    return strategy;
}

// Turns regrets into a strategy in place: negatives clamp to zero,
// then normalize, uniform when nothing is positive.
inline void regret_matching_in_place(std::span<double> strategy) {
    for (double& value : strategy) value = std::max(value, 0.0);
    normalize_or_uniform(strategy, 0.0);
}

// Regret matching written into strategy;
// both spans must have the same length.
inline void regret_matching_strategy_into(
    std::span<const double> cumulative_regrets, std::span<double> strategy) {
    for (std::size_t i = 0; i < cumulative_regrets.size(); ++i)
        strategy[i] = cumulative_regrets[i];
    regret_matching_in_place(strategy);
}

// Regret matching returned as its own vector.
inline std::vector<double> regret_matching_strategy(
    std::span<const double> cumulative_regrets) {
    std::vector<double> strategy(cumulative_regrets.size());
    regret_matching_strategy_into(cumulative_regrets, strategy);
    return strategy;
}

// CFR+ update: adds the increments and floors each regret at zero.
inline void accumulate_regret_plus(std::span<double> cumulative_regrets,
                                   std::span<const double> increments) {
    for (std::size_t i = 0; i < cumulative_regrets.size(); ++i) {
        cumulative_regrets[i] =
            std::max(cumulative_regrets[i] + increments[i], 0.0);
    }
}

}
