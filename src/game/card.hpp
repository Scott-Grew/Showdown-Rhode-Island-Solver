// Card ids for the 52-card deck. A card id is rank * kSuitCount + suit,
// rank 0..12 (deuce low, ace high) and suit 0..3.

#pragma once

#include <cstdint>
#include <string>

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

// Two-character card text such as "As" or "Tc".
inline std::string rih_card_name(int card) {
    static const char* const kRankNames[] = {"2", "3", "4", "5", "6", "7", "8",
                                             "9", "T", "J", "Q", "K", "A"};
    static const char* const kSuitNames[] = {"c", "d", "h", "s"};
    Card typed_card = static_cast<Card>(card);
    return std::string(kRankNames[card_rank(typed_card)]) +
           kSuitNames[card_suit(typed_card)];
}

}
