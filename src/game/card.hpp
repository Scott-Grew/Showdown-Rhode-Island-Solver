#pragma once

#include <cstdint>

namespace cfr {

// Card encoding shared by cfr::game (deal/apply) and cfr::eval (hand ranking):
// a single byte, 0..51, rank-major — card = rank * kSuitCount + suit, with
// rank 0 = deuce .. rank 12 = ace. Suit identity carries no ordering; it only
// matters for flush detection in the evaluator.
using Card = std::uint8_t;

constexpr int kRankCount = 13;
constexpr int kSuitCount = 4;
constexpr int kCardCount = kRankCount * kSuitCount;

constexpr Card make_card(int rank, int suit) {
    return static_cast<Card>(rank * kSuitCount + suit);
}

constexpr int card_rank(Card card) {
    return card / kSuitCount;
}

constexpr int card_suit(Card card) {
    return card % kSuitCount;
}

}  // namespace cfr
