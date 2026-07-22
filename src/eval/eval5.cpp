#include "eval/eval5.hpp"

#include <array>
#include <bit>
#include <cstdint>
#include <initializer_list>

namespace cfr::eval {

namespace {

using cfr::card_rank;
using cfr::card_suit;
using cfr::kRankCount;

constexpr int kHighCard = 0;
constexpr int kOnePair = 1;
constexpr int kTwoPair = 2;
constexpr int kThreeOfAKind = 3;
constexpr int kStraight = 4;
constexpr int kFlush = 5;
constexpr int kFullHouse = 6;
constexpr int kFourOfAKind = 7;
constexpr int kStraightFlush = 8;

constexpr int kCategoryBudget = 2860;

constexpr int binomial(int n, int k) {
    if (k < 0 || k > n) return 0;
    int result = 1;
    for (int i = 0; i < k; ++i) {
        result = result * (n - i) / (i + 1);
    }
    return result;
}

template <std::size_t N>
int colex_index(const std::array<int, N>& ascending_subset) {
    int index = 0;
    for (std::size_t i = 0; i < N; ++i) {
        index += binomial(ascending_subset[i], static_cast<int>(i) + 1);
    }
    return index;
}

int compact_rank(int rank, std::initializer_list<int> excluded) {
    int shift = 0;
    for (int excluded_rank : excluded) {
        if (excluded_rank < rank) ++shift;
    }
    return rank - shift;
}

constexpr std::uint32_t kWheelBitmask = 0x100F;

}

HandRank evaluate_5card(const Card* cards) {
    std::array<int, kRankCount> rank_counts = {};
    std::uint32_t rank_bitmask = 0;
    bool is_flush = true;
    int first_suit = card_suit(cards[0]);
    for (int i = 0; i < 5; ++i) {
        int rank = card_rank(cards[i]);
        ++rank_counts[rank];
        rank_bitmask |= 1u << rank;
        if (card_suit(cards[i]) != first_suit) is_flush = false;
    }

    bool all_distinct_ranks = std::popcount(rank_bitmask) == 5;
    bool is_wheel = rank_bitmask == kWheelBitmask;
    bool is_straight = false;
    int straight_high = -1;
    if (all_distinct_ranks) {
        if (is_wheel) {
            is_straight = true;
            straight_high = 3;
        } else {
            int lowest_set_bit = std::countr_zero(rank_bitmask);
            int highest_set_bit = 31 - std::countl_zero(rank_bitmask);
            if (highest_set_bit - lowest_set_bit == 4) {
                is_straight = true;
                straight_high = highest_set_bit;
            }
        }
    }

    if (is_straight && is_flush) {
        return static_cast<HandRank>(kStraightFlush * kCategoryBudget + straight_high);
    }
    if (is_straight) {
        return static_cast<HandRank>(kStraight * kCategoryBudget + straight_high);
    }

    int quad_rank = -1;
    int trip_rank = -1;
    std::array<int, 2> pair_ranks = {-1, -1};
    int pair_count = 0;
    std::array<int, 5> single_ranks = {-1, -1, -1, -1, -1};
    int single_count = 0;
    for (int rank = 0; rank < kRankCount; ++rank) {
        switch (rank_counts[rank]) {
            case 4: quad_rank = rank; break;
            case 3: trip_rank = rank; break;
            case 2: pair_ranks[pair_count++] = rank; break;
            case 1: single_ranks[single_count++] = rank; break;
        }
    }

    if (is_flush) {
        return static_cast<HandRank>(kFlush * kCategoryBudget + colex_index(single_ranks));
    }
    if (quad_rank != -1) {
        return static_cast<HandRank>(kFourOfAKind * kCategoryBudget + quad_rank * kRankCount + single_ranks[0]);
    }
    if (trip_rank != -1 && pair_count == 1) {
        return static_cast<HandRank>(kFullHouse * kCategoryBudget + trip_rank * kRankCount + pair_ranks[0]);
    }
    if (trip_rank != -1) {
        std::array<int, 2> kickers = {compact_rank(single_ranks[0], {trip_rank}),
                                       compact_rank(single_ranks[1], {trip_rank})};
        return static_cast<HandRank>(kThreeOfAKind * kCategoryBudget + trip_rank * binomial(12, 2) +
                                      colex_index(kickers));
    }
    if (pair_count == 2) {
        int kicker = compact_rank(single_ranks[0], {pair_ranks[0], pair_ranks[1]});
        return static_cast<HandRank>(kTwoPair * kCategoryBudget + colex_index(pair_ranks) * 11 + kicker);
    }
    if (pair_count == 1) {
        std::array<int, 3> kickers = {compact_rank(single_ranks[0], {pair_ranks[0]}),
                                       compact_rank(single_ranks[1], {pair_ranks[0]}),
                                       compact_rank(single_ranks[2], {pair_ranks[0]})};
        return static_cast<HandRank>(kOnePair * kCategoryBudget + pair_ranks[0] * binomial(12, 3) +
                                      colex_index(kickers));
    }
    return static_cast<HandRank>(kHighCard * kCategoryBudget + colex_index(single_ranks));
}

}
