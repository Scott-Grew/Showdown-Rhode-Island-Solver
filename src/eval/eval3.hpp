#pragma once

#include <cstdint>

#include "game/card.hpp"

namespace cfr::eval {

// Larger beats smaller; equal values tie.
using HandRank = std::uint16_t;

// Strength of a three-card hand. cards must point at three
// distinct card ids.
HandRank evaluate_3card(const Card* cards);

}
