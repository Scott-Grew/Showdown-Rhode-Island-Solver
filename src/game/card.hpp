// Card ids for the 52-card deck. A card id is rank * kSuitCount + suit,
// rank 0..12 (deuce low, ace high) and suit 0..3.

#pragma once

#include <cstdint>

namespace cfr {

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

}
