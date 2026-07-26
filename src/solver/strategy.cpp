#include "solver/strategy.hpp"

#include <algorithm>
#include <cstddef>

namespace cfr::solver {

void regret_matching_strategy_into(std::span<const double> cumulative_regrets, std::span<double> strategy) {
    double positive_regret_sum = 0.0;
    for (std::size_t i = 0; i < cumulative_regrets.size(); ++i) {
        strategy[i] = std::max(cumulative_regrets[i], 0.0);
        positive_regret_sum += strategy[i];
    }

    if (positive_regret_sum > 0.0) {
        for (std::size_t i = 0; i < strategy.size(); ++i) {
            strategy[i] /= positive_regret_sum;
        }
        return;
    }

    double uniform_probability = 1.0 / static_cast<double>(strategy.size());
    for (double& probability : strategy) {
        probability = uniform_probability;
    }
}

std::vector<double> regret_matching_strategy(std::span<const double> cumulative_regrets) {
    std::vector<double> strategy(cumulative_regrets.size());
    regret_matching_strategy_into(cumulative_regrets, strategy);
    return strategy;
}

}
