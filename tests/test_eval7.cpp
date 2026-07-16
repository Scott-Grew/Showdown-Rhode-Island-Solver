#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <array>
#include <cstddef>
#include <random>
#include <vector>

#include "eval/eval5.hpp"
#include "eval/eval7.hpp"
#include "game/card.hpp"

using cfr::Card;
using cfr::kCardCount;
using cfr::eval::evaluate_5card;
using cfr::eval::evaluate_7card;
using cfr::eval::evaluate_7card_batch;
using cfr::eval::HandRank;

namespace {

std::array<Card, 7> random_seven_card_hand(std::mt19937_64& rng) {
    std::uniform_int_distribution<int> card_distribution(0, kCardCount - 1);
    std::array<Card, 7> hand;
    std::array<bool, kCardCount> used = {};
    for (Card& card : hand) {
        int drawn;
        do {
            drawn = card_distribution(rng);
        } while (used[drawn]);
        used[drawn] = true;
        card = static_cast<Card>(drawn);
    }
    return hand;
}

// All C(7,5) = 21 ways to choose 5 of 7 card slots. Deliberately a separate
// copy from eval7.cpp's own subset table -- this test exists to catch a bug
// in evaluate_7card, so it can't rely on the code under test to supply the
// list it's checked against.
constexpr std::array<std::array<int, 5>, 21> kFiveOfSevenSubsets = {{
    {0, 1, 2, 3, 4}, {0, 1, 2, 3, 5}, {0, 1, 2, 3, 6}, {0, 1, 2, 4, 5}, {0, 1, 2, 4, 6},
    {0, 1, 2, 5, 6}, {0, 1, 3, 4, 5}, {0, 1, 3, 4, 6}, {0, 1, 3, 5, 6}, {0, 1, 4, 5, 6},
    {0, 2, 3, 4, 5}, {0, 2, 3, 4, 6}, {0, 2, 3, 5, 6}, {0, 2, 4, 5, 6}, {0, 3, 4, 5, 6},
    {1, 2, 3, 4, 5}, {1, 2, 3, 4, 6}, {1, 2, 3, 5, 6}, {1, 2, 4, 5, 6}, {1, 3, 4, 5, 6},
    {2, 3, 4, 5, 6},
}};

HandRank brute_force_best_of_21(const std::array<Card, 7>& hand) {
    HandRank best = 0;
    for (const auto& subset : kFiveOfSevenSubsets) {
        std::array<Card, 5> five_card_hand = {hand[subset[0]], hand[subset[1]], hand[subset[2]], hand[subset[3]],
                                               hand[subset[4]]};
        best = std::max(best, evaluate_5card(five_card_hand.data()));
    }
    return best;
}

}  // namespace

TEST_CASE("eval7: evaluate_7card equals the best of 21 5-card subsets over 10^5 random boards") {  // V5
    std::mt19937_64 rng(0xE7A17CA2D);
    constexpr int kBoardCount = 100'000;
    for (int trial = 0; trial < kBoardCount; ++trial) {
        std::array<Card, 7> hand = random_seven_card_hand(rng);
        REQUIRE(evaluate_7card(hand.data()) == brute_force_best_of_21(hand));
    }
}

TEST_CASE("eval7: batch results equal scalar evaluate_7card over 10^5 random boards") {  // V9
    std::mt19937_64 rng(0xBA7C44ED);
    constexpr std::size_t kBoardCount = 100'000;
    constexpr int kCardsPerHand = 7;

    std::vector<Card> hands(kBoardCount * kCardsPerHand);
    std::vector<HandRank> scalar_ranks(kBoardCount);
    for (std::size_t board = 0; board < kBoardCount; ++board) {
        std::array<Card, 7> hand = random_seven_card_hand(rng);
        std::copy(hand.begin(), hand.end(), hands.begin() + static_cast<std::ptrdiff_t>(board * kCardsPerHand));
        scalar_ranks[board] = evaluate_7card(hand.data());
    }

    std::vector<HandRank> batch_ranks(kBoardCount);
    evaluate_7card_batch(hands.data(), kBoardCount, batch_ranks.data());

    REQUIRE(batch_ranks == scalar_ranks);
}
