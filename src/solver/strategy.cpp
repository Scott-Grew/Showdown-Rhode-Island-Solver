#include "solver/strategy.hpp"

#include <algorithm>
#include <cstddef>

namespace cfr::solver {

std::vector<double> regret_matching_strategy(const std::vector<double>& cumulative_regrets) {
    std::vector<double> positive_regrets(cumulative_regrets.size());
    double positive_regret_sum = 0.0;
    for (std::size_t i = 0; i < cumulative_regrets.size(); ++i) {
        positive_regrets[i] = std::max(cumulative_regrets[i], 0.0);
        positive_regret_sum += positive_regrets[i];
    }

    std::vector<double> strategy(cumulative_regrets.size());
    if (positive_regret_sum > 0.0) {
        for (std::size_t i = 0; i < strategy.size(); ++i) {
            strategy[i] = positive_regrets[i] / positive_regret_sum;
        }
        return strategy;
    }

    double uniform_probability = 1.0 / static_cast<double>(strategy.size());
    for (double& probability : strategy) {
        probability = uniform_probability;
    }
    return strategy;
}

}
