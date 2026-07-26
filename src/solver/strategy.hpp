#pragma once

#include <algorithm>
#include <cstddef>
#include <map>
#include <span>
#include <vector>

#include "game/game.hpp"

namespace cfr::solver {

using StrategyProfile = std::map<game::InfoSetKey, std::vector<double>>;

void regret_matching_strategy_into(std::span<const double> cumulative_regrets, std::span<double> strategy);

std::vector<double> regret_matching_strategy(std::span<const double> cumulative_regrets);

inline void accumulate_regret_plus(std::span<double> cumulative_regrets, std::span<const double> increments) {
    for (std::size_t i = 0; i < cumulative_regrets.size(); ++i) {
        cumulative_regrets[i] = std::max(cumulative_regrets[i] + increments[i], 0.0);
    }
}

}
