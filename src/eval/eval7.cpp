#include "eval/eval7.hpp"

#include <algorithm>
#include <array>
#include <cstddef>

namespace cfr::eval {

namespace {

constexpr int kCardsPerHand = 7;

constexpr std::array<std::array<int, 5>, 21> kFiveOfSevenSubsets = {{
    {0, 1, 2, 3, 4}, {0, 1, 2, 3, 5}, {0, 1, 2, 3, 6}, {0, 1, 2, 4, 5}, {0, 1, 2, 4, 6},
    {0, 1, 2, 5, 6}, {0, 1, 3, 4, 5}, {0, 1, 3, 4, 6}, {0, 1, 3, 5, 6}, {0, 1, 4, 5, 6},
    {0, 2, 3, 4, 5}, {0, 2, 3, 4, 6}, {0, 2, 3, 5, 6}, {0, 2, 4, 5, 6}, {0, 3, 4, 5, 6},
    {1, 2, 3, 4, 5}, {1, 2, 3, 4, 6}, {1, 2, 3, 5, 6}, {1, 2, 4, 5, 6}, {1, 3, 4, 5, 6},
    {2, 3, 4, 5, 6},
}};

}

HandRank evaluate_7card(const Card* cards) {
    HandRank best = 0;
    for (const auto& subset : kFiveOfSevenSubsets) {
        std::array<Card, 5> five_card_hand = {cards[subset[0]], cards[subset[1]], cards[subset[2]],
                                               cards[subset[3]], cards[subset[4]]};
        best = std::max(best, evaluate_5card(five_card_hand.data()));
    }
    return best;
}

void evaluate_7card_batch(const Card* hands, std::size_t hand_count, HandRank* ranks_out) {
    for (std::size_t hand_index = 0; hand_index < hand_count; ++hand_index) {
        ranks_out[hand_index] = evaluate_7card(hands + hand_index * kCardsPerHand);
    }
}

}
