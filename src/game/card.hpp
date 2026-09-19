#pragma once

#include <cstdint>

namespace cfr {

using Card = std::uint8_t;

// Deck shape. A card id is rank * kSuitCount + suit.
constexpr int kRankCount = 13;
constexpr int kSuitCount = 4;
constexpr int kCardCount = kRankCount * kSuitCount;

// Card id from rank 0..12 (two low, ace high) and suit 0..3.
constexpr Card make_card(int rank, int suit) {
    return static_cast<Card>(rank * kSuitCount + suit);
}

// Rank of a card id, 0..12, ace high.
constexpr int card_rank(Card card) {
    return card / kSuitCount;
}

// Suit of a card id, 0..3.
constexpr int card_suit(Card card) {
    return card % kSuitCount;
}

}
