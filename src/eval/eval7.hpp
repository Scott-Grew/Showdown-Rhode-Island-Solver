#pragma once

#include <cstddef>

#include "eval/eval5.hpp"
#include "game/card.hpp"

namespace cfr::eval {

// Ranks a 7-card hand as the best of its C(7,5) = 21 5-card subsets.
// `cards` must point to 7 distinct cfr::Card values (0..51).
HandRank evaluate_7card(const Card* cards);

// Ranks `hand_count` 7-card hands in one call. `hands` holds `hand_count`
// contiguous 7-card blocks (7 * hand_count cfr::Card values); `ranks_out`
// must have room for `hand_count` HandRank values. Always produces exactly
// the same results as calling evaluate_7card per hand (V9) -- the AVX2 path
// below is a throughput optimization of the same computation, not a
// different one.
void evaluate_7card_batch(const Card* hands, std::size_t hand_count, HandRank* ranks_out);

}  // namespace cfr::eval
