#pragma once

#include <algorithm>
#include <cstddef>
#include <map>
#include <vector>

#include "game/game.hpp"

namespace cfr::solver {

using StrategyProfile = std::map<game::InfoSetKey, std::vector<double>>;

std::vector<double> regret_matching_strategy(const std::vector<double>& cumulative_regrets);

template <typename IncrementContainer>
void accumulate_regret_plus(std::vector<double>& cumulative_regrets, const IncrementContainer& increments) {
    for (std::size_t i = 0; i < cumulative_regrets.size(); ++i) {
        cumulative_regrets[i] = std::max(cumulative_regrets[i] + increments[i], 0.0);
    }
}

}
