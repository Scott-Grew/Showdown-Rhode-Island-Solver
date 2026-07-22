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
