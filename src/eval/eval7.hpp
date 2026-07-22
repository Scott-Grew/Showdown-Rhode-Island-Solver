#pragma once

#include <cstddef>

#include "eval/eval5.hpp"
#include "game/card.hpp"

namespace cfr::eval {

HandRank evaluate_7card(const Card* cards);

void evaluate_7card_batch(const Card* hands, std::size_t hand_count, HandRank* ranks_out);

}
