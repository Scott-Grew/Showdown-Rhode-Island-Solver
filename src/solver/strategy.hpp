#pragma once

#include <algorithm>
#include <cstddef>
#include <map>
#include <span>
#include <vector>

#include "game/game.hpp"

namespace cfr::solver {

using StrategyProfile = std::map<game::InfoSetKey, std::vector<double>>;

inline constexpr double kStrategySumEpsilon = 1e-12;

inline void normalize_or_uniform(std::span<double> values, double epsilon = kStrategySumEpsilon) {
    double total = 0.0;
    for (double value : values) total += value;

    if (total > epsilon) {
        for (double& value : values) value /= total;
        return;
    }
    double uniform_probability = 1.0 / static_cast<double>(values.size());
    for (double& value : values) value = uniform_probability;
}

inline std::vector<double> average_from_sums(std::span<const double> strategy_sum) {
    std::vector<double> strategy(strategy_sum.begin(), strategy_sum.end());
    normalize_or_uniform(strategy);
    return strategy;
}

void regret_matching_strategy_into(std::span<const double> cumulative_regrets, std::span<double> strategy);

std::vector<double> regret_matching_strategy(std::span<const double> cumulative_regrets);

inline void accumulate_regret_plus(std::span<double> cumulative_regrets, std::span<const double> increments) {
    for (std::size_t i = 0; i < cumulative_regrets.size(); ++i) {
        cumulative_regrets[i] = std::max(cumulative_regrets[i] + increments[i], 0.0);
    }
}

}
