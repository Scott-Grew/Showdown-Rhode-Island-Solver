#pragma once

#include <map>
#include <vector>

#include "game/game.hpp"

namespace cfr::solver {

using StrategyProfile = std::map<game::InfoSetKey, std::vector<double>>;

std::vector<double> regret_matching_strategy(const std::vector<double>& cumulative_regrets);

void accumulate_regret_plus(std::vector<double>& cumulative_regrets, const std::vector<double>& increments);

}
