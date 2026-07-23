#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <array>
#include <cstddef>
#include <numeric>
#include <vector>

#include "eval/eval3.hpp"
#include "eval/naive_ref.hpp"
#include "game/card.hpp"

using cfr::Card;
using cfr::kCardCount;
using cfr::make_card;
using cfr::eval::evaluate_3card;
using cfr::eval::HandRank;
using cfr::eval::naive::naive_evaluate_3card;
using cfr::eval::naive::NaiveRank;

TEST_CASE("RIH eval3 category order: straight-flush > trips > straight > flush > pair > high-card") {
    std::array<Card, 3> straight_flush = {make_card(3, 0), make_card(4, 0), make_card(5, 0)};
    std::array<Card, 3> trips = {make_card(5, 0), make_card(5, 1), make_card(5, 2)};
    std::array<Card, 3> straight = {make_card(3, 0), make_card(4, 1), make_card(5, 2)};
    std::array<Card, 3> flush = {make_card(0, 0), make_card(1, 0), make_card(3, 0)};
    std::array<Card, 3> pair = {make_card(5, 0), make_card(5, 1), make_card(2, 2)};
    std::array<Card, 3> high_card = {make_card(0, 0), make_card(2, 1), make_card(5, 2)};

    REQUIRE(evaluate_3card(straight_flush.data()) > evaluate_3card(trips.data()));
    REQUIRE(evaluate_3card(trips.data()) > evaluate_3card(straight.data()));
    REQUIRE(evaluate_3card(straight.data()) > evaluate_3card(flush.data()));
    REQUIRE(evaluate_3card(flush.data()) > evaluate_3card(pair.data()));
    REQUIRE(evaluate_3card(pair.data()) > evaluate_3card(high_card.data()));

    REQUIRE(naive_evaluate_3card(trips.data()) < naive_evaluate_3card(straight_flush.data()));
    REQUIRE(naive_evaluate_3card(straight.data()) < naive_evaluate_3card(trips.data()));
    REQUIRE(naive_evaluate_3card(flush.data()) < naive_evaluate_3card(straight.data()));
    REQUIRE(naive_evaluate_3card(pair.data()) < naive_evaluate_3card(flush.data()));
    REQUIRE(naive_evaluate_3card(high_card.data()) < naive_evaluate_3card(pair.data()));
}

TEST_CASE("RIH eval3 inversions vs standard poker: trips beats straight and flush; straight beats flush") {
    std::array<Card, 3> trips = {make_card(6, 0), make_card(6, 1), make_card(6, 2)};
    std::array<Card, 3> straight = {make_card(4, 0), make_card(5, 1), make_card(6, 2)};
    std::array<Card, 3> flush = {make_card(0, 3), make_card(2, 3), make_card(7, 3)};

    REQUIRE(evaluate_3card(trips.data()) > evaluate_3card(straight.data()));
    REQUIRE(evaluate_3card(trips.data()) > evaluate_3card(flush.data()));
    REQUIRE(evaluate_3card(straight.data()) > evaluate_3card(flush.data()));

    REQUIRE(naive_evaluate_3card(straight.data()) < naive_evaluate_3card(trips.data()));
    REQUIRE(naive_evaluate_3card(flush.data()) < naive_evaluate_3card(trips.data()));
    REQUIRE(naive_evaluate_3card(flush.data()) < naive_evaluate_3card(straight.data()));
}

TEST_CASE("RIH eval3 wheel: A-2-3 is the lowest straight, Q-K-A the highest") {
    std::array<Card, 3> wheel = {make_card(12, 0), make_card(0, 1), make_card(1, 2)};
    std::array<Card, 3> two_three_four = {make_card(0, 0), make_card(1, 1), make_card(2, 2)};
    std::array<Card, 3> broadway = {make_card(10, 0), make_card(11, 1), make_card(12, 2)};

    REQUIRE(naive_evaluate_3card(wheel.data()).category == 3);
    REQUIRE(naive_evaluate_3card(two_three_four.data()).category == 3);
    REQUIRE(naive_evaluate_3card(broadway.data()).category == 3);

    REQUIRE(evaluate_3card(wheel.data()) < evaluate_3card(two_three_four.data()));
    REQUIRE(evaluate_3card(two_three_four.data()) < evaluate_3card(broadway.data()));
    REQUIRE(naive_evaluate_3card(wheel.data()) < naive_evaluate_3card(two_three_four.data()));
    REQUIRE(naive_evaluate_3card(two_three_four.data()) < naive_evaluate_3card(broadway.data()));
}

TEST_CASE("RIH eval3 fast path matches naive reference ordering over all C(52,3) hands", "[slow]") {
    constexpr std::size_t kExpectedHandCount = 22'100;

    std::vector<HandRank> fast_ranks;
    std::vector<NaiveRank> naive_ranks;
    fast_ranks.reserve(kExpectedHandCount);
    naive_ranks.reserve(kExpectedHandCount);

    for (int c0 = 0; c0 < kCardCount; ++c0) {
        for (int c1 = c0 + 1; c1 < kCardCount; ++c1) {
            for (int c2 = c1 + 1; c2 < kCardCount; ++c2) {
                std::array<Card, 3> hand = {static_cast<Card>(c0), static_cast<Card>(c1), static_cast<Card>(c2)};
                fast_ranks.push_back(evaluate_3card(hand.data()));
                naive_ranks.push_back(naive_evaluate_3card(hand.data()));
            }
        }
    }

    REQUIRE(fast_ranks.size() == kExpectedHandCount);

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
