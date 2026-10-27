// Three-card hand evaluator. Rhode Island showdowns are its only
// caller in src.

#pragma once

#include <cstdint>

#include "game/card.hpp"

namespace cfr::eval {

// Larger beats smaller; equal values tie.
using HandRank = std::uint16_t;

// Strength of a three-card hand. cards must point at three
// distinct card ids.
HandRank evaluate_3card(const Card* cards);

// evaluate_3card, read from a table that function filled. The
// index is the colexicographic rank of the three cards sorted
// ascending. cards must point at three distinct card ids.
HandRank lookup_3card(const Card* cards);

}
