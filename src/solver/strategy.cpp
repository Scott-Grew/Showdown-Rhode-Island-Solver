#include "solver/strategy.hpp"

#include <algorithm>
#include <cstddef>

namespace cfr::solver {

void regret_matching_strategy_into(std::span<const double> cumulative_regrets, std::span<double> strategy) {
    for (std::size_t i = 0; i < cumulative_regrets.size(); ++i) {
        strategy[i] = std::max(cumulative_regrets[i], 0.0);
    }
    normalize_or_uniform(strategy, 0.0);
}

std::vector<double> regret_matching_strategy(std::span<const double> cumulative_regrets) {
    std::vector<double> strategy(cumulative_regrets.size());
    regret_matching_strategy_into(cumulative_regrets, strategy);
    return strategy;
}

}
