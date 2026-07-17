#pragma once

#include <map>
#include <vector>

#include "game/game.hpp"

namespace cfr::solver {

// Behavioral strategy: infoset -> action probabilities, indexed identically
// to game.legal_actions(state) order at that infoset. Probabilities sum to
// 1. std::map (not unordered) -- deterministic iteration is what makes two
// independent solver runs produce byte-identical output (V14); these games
// are small enough that ordered-map lookup cost is irrelevant at M1.
using StrategyProfile = std::map<game::InfoSetKey, std::vector<double>>;

// Regret matching (Hart & Mas-Colell): returns a distribution proportional
// to the positive part of cumulative_regrets, normalized to sum to 1. When
// no entry is positive (all <= 0), returns the uniform distribution over
// the same number of actions instead of dividing by zero -- this is the
// well-defined fallback for infosets that haven't shown any action to be
// worth favoring yet.
std::vector<double> regret_matching_strategy(const std::vector<double>& cumulative_regrets);

}  // namespace cfr::solver
