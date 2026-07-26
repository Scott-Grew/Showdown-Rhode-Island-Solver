#pragma once

#include <cstdint>

#include "game/card.hpp"

namespace cfr::eval {

using HandRank = std::uint16_t;

HandRank evaluate_3card(const Card* cards);

}
