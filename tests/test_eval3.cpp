// Three-card order against the poker ranking, the wheel, and the
// naive reference over every C(52,3) hand. The same sweep checks the
// rank table lookup_3card filled from evaluate_3card.

#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <array>
#include <cstddef>
#include <numeric>
#include <vector>

#include "eval/eval3.hpp"
#include "game/card.hpp"
#include "naive_ref.hpp"

using cfr::Card;
using cfr::kCardCount;
using cfr::make_card;
using cfr::eval::evaluate_3card;
using cfr::eval::HandRank;
using cfr::eval::lookup_3card;
using cfr::eval::naive::naive_evaluate_3card;
using cfr::eval::naive::NaiveRank;

TEST_CASE("RIH eval3 ranks categories in three-card poker order") {
    std::array<std::array<Card, 3>, 6> hands = {{
        {make_card(3, 0), make_card(4, 0), make_card(5, 0)},
        {make_card(5, 0), make_card(5, 1), make_card(5, 2)},
        {make_card(3, 0), make_card(4, 1), make_card(5, 2)},
        {make_card(0, 0), make_card(1, 0), make_card(3, 0)},
        {make_card(5, 0), make_card(5, 1), make_card(2, 2)},
        {make_card(0, 0), make_card(2, 1), make_card(5, 2)},
    }};
    for (std::size_t i = 0; i + 1 < hands.size(); ++i) {
        REQUIRE(evaluate_3card(hands[i].data()) >
                evaluate_3card(hands[i + 1].data()));
    }
}

TEST_CASE("RIH eval3 wheel: A-2-3 is the lowest straight, Q-K-A the highest") {
    std::array<Card, 3> wheel = {make_card(12, 0), make_card(0, 1),
                                 make_card(1, 2)};
    std::array<Card, 3> two_three_four = {make_card(0, 0), make_card(1, 1),
                                          make_card(2, 2)};
    std::array<Card, 3> broadway = {make_card(10, 0), make_card(11, 1),
                                    make_card(12, 2)};

    REQUIRE(naive_evaluate_3card(wheel.data()).category == 3);
    REQUIRE(naive_evaluate_3card(two_three_four.data()).category == 3);
    REQUIRE(naive_evaluate_3card(broadway.data()).category == 3);

    REQUIRE(evaluate_3card(wheel.data()) <
            evaluate_3card(two_three_four.data()));
    REQUIRE(evaluate_3card(two_three_four.data()) <
            evaluate_3card(broadway.data()));
}

TEST_CASE("RIH eval3 matches the naive reference order over all C(52,3) hands",
          "[slow]") {
    constexpr std::size_t kExpectedHandCount = 22'100;

    std::vector<HandRank> fast_ranks;
    std::vector<NaiveRank> naive_ranks;
    fast_ranks.reserve(kExpectedHandCount);
    naive_ranks.reserve(kExpectedHandCount);
    int lookup_mismatches = 0;

    for (int c0 = 0; c0 < kCardCount; ++c0) {
        for (int c1 = c0 + 1; c1 < kCardCount; ++c1) {
            for (int c2 = c1 + 1; c2 < kCardCount; ++c2) {
                std::array<Card, 3> hand = {static_cast<Card>(c0),
                                            static_cast<Card>(c1),
                                            static_cast<Card>(c2)};
                HandRank ranked = evaluate_3card(hand.data());
                fast_ranks.push_back(ranked);
                naive_ranks.push_back(naive_evaluate_3card(hand.data()));
                std::array<Card, 3> rotated = {hand[1], hand[2], hand[0]};
                if (lookup_3card(hand.data()) != ranked ||
                    lookup_3card(rotated.data()) != ranked)
                    ++lookup_mismatches;
            }
        }
    }

    REQUIRE(fast_ranks.size() == kExpectedHandCount);
    REQUIRE(lookup_mismatches == 0);

    std::vector<std::size_t> order(fast_ranks.size());
    std::iota(order.begin(), order.end(), std::size_t{0});
    std::sort(order.begin(), order.end(), [&](std::size_t a, std::size_t b) {
        return fast_ranks[a] < fast_ranks[b];
    });

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
