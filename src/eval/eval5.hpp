#pragma once

#include <cstdint>

#include "game/card.hpp"

namespace cfr::eval {

// HandRank: greater = stronger, equal = tie. The numeric scale itself
// carries no external meaning -- only comparisons between evaluate_5card /
// evaluate_7card outputs matter (see eval5.cpp for the packing scheme).
using HandRank = std::uint16_t;

// Ranks a 5-card hand. `cards` must point to 5 distinct cfr::Card values
// (0..51) -- a real deal can never hand out a duplicate card, so that's not
// checked at this layer.
HandRank evaluate_5card(const Card* cards);

}  // namespace cfr::eval
