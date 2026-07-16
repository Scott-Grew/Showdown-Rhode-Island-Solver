#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <array>
#include <cstddef>
#include <numeric>
#include <random>
#include <vector>

#include "eval/eval5.hpp"
#include "eval/naive_ref.hpp"
#include "game/card.hpp"

using cfr::Card;
using cfr::kCardCount;
using cfr::make_card;
using cfr::eval::evaluate_5card;
using cfr::eval::HandRank;
using cfr::eval::naive::naive_evaluate_5card;
using cfr::eval::naive::NaiveRank;

TEST_CASE("eval5: wheel (A-2-3-4-5) ranks below a 6-high straight, not above it") {  // V4 edge case
    // Ace-low straights are the classic evaluator bug: an implementation that
    // forgets the special case treats the ace as high and misranks the wheel
    // as the strongest straight instead of the weakest.
    std::array<Card, 5> wheel = {make_card(12, 0), make_card(0, 1), make_card(1, 2), make_card(2, 3), make_card(3, 0)};
    std::array<Card, 5> six_high = {make_card(1, 0), make_card(2, 1), make_card(3, 2), make_card(4, 3), make_card(5, 0)};
    std::array<Card, 5> broadway = {make_card(8, 0), make_card(9, 1), make_card(10, 2), make_card(11, 3), make_card(12, 1)};

    REQUIRE(naive_evaluate_5card(wheel.data()).category == 4);
    REQUIRE(naive_evaluate_5card(six_high.data()).category == 4);
    REQUIRE(naive_evaluate_5card(broadway.data()).category == 4);

    REQUIRE(evaluate_5card(wheel.data()) < evaluate_5card(six_high.data()));
    REQUIRE(evaluate_5card(six_high.data()) < evaluate_5card(broadway.data()));
    REQUIRE(naive_evaluate_5card(wheel.data()) < naive_evaluate_5card(six_high.data()));
    REQUIRE(naive_evaluate_5card(six_high.data()) < naive_evaluate_5card(broadway.data()));
}

TEST_CASE("eval5: fast path matches naive reference ordering over all C(52,5) hands", "[slow]") {  // V4
    constexpr std::size_t kExpectedHandCount = 2'598'960;  // C(52,5)

    std::vector<HandRank> fast_ranks;
    std::vector<NaiveRank> naive_ranks;
    fast_ranks.reserve(kExpectedHandCount);
    naive_ranks.reserve(kExpectedHandCount);

    for (int c0 = 0; c0 < kCardCount; ++c0) {
        for (int c1 = c0 + 1; c1 < kCardCount; ++c1) {
            for (int c2 = c1 + 1; c2 < kCardCount; ++c2) {
                for (int c3 = c2 + 1; c3 < kCardCount; ++c3) {
                    for (int c4 = c3 + 1; c4 < kCardCount; ++c4) {
                        std::array<Card, 5> hand = {static_cast<Card>(c0), static_cast<Card>(c1),
                                                     static_cast<Card>(c2), static_cast<Card>(c3),
                                                     static_cast<Card>(c4)};
                        fast_ranks.push_back(evaluate_5card(hand.data()));
                        naive_ranks.push_back(naive_evaluate_5card(hand.data()));
                    }
                }
            }
        }
    }

    REQUIRE(fast_ranks.size() == kExpectedHandCount);

    // Order-agreement check: sort hand indices by the fast path's rank, then
    // walk the sorted order once. Equal-fast-rank neighbors must be
    // equal-naive-rank (same tie class); different-fast-rank neighbors must
    // be strictly increasing in naive rank too. A single adjacent pass
    // suffices for the whole array by transitivity of < and ==, and avoids
    // 2.6M individual Catch2 assertions.
    std::vector<std::size_t> order(fast_ranks.size());
    std::iota(order.begin(), order.end(), std::size_t{0});
    std::sort(order.begin(), order.end(),
              [&](std::size_t a, std::size_t b) { return fast_ranks[a] < fast_ranks[b]; });

    std::size_t first_mismatch = order.size();
    for (std::size_t i = 1; i < order.size(); ++i) {
        std::size_t previous = order[i - 1];
        std::size_t current = order[i];
        bool consistent = fast_ranks[current] == fast_ranks[previous]
                               ? naive_ranks[current] == naive_ranks[previous]
                               : naive_ranks[previous] < naive_ranks[current];
        if (!consistent) {
            first_mismatch = i;
            break;
        }
    }
    REQUIRE(first_mismatch == order.size());
}

TEST_CASE("eval5: fast path matches naive reference ordering over 10^6 random pairs") {  // V4 spot-check
    std::mt19937_64 rng(0xC5F0'0D5EED);
    std::uniform_int_distribution<int> card_distribution(0, kCardCount - 1);

    auto random_hand = [&]() {
        std::array<Card, 5> hand;
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
    };

    constexpr int kPairCount = 1'000'000;
    for (int trial = 0; trial < kPairCount; ++trial) {
        std::array<Card, 5> hand_a = random_hand();
        std::array<Card, 5> hand_b = random_hand();

        HandRank fast_a = evaluate_5card(hand_a.data());
        HandRank fast_b = evaluate_5card(hand_b.data());
        NaiveRank naive_a = naive_evaluate_5card(hand_a.data());
        NaiveRank naive_b = naive_evaluate_5card(hand_b.data());

        bool consistent = fast_a == fast_b ? naive_a == naive_b
                           : fast_a < fast_b ? naive_a < naive_b
                                             : naive_b < naive_a;
        if (!consistent) {
            REQUIRE(consistent);
            break;
        }
    }
}
