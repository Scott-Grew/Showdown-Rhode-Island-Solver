#include "eval/eval3.hpp"

#include <array>
#include <bit>
#include <cstdint>

namespace cfr::eval {

namespace {

using cfr::card_rank;
using cfr::card_suit;
using cfr::kRankCount;

// Hand categories, weakest first. Straights outrank flushes in
// three-card poker.
constexpr int kHighCard = 0;
constexpr int kPair = 1;
constexpr int kFlush = 2;
constexpr int kStraight = 3;
constexpr int kTrips = 4;
constexpr int kStraightFlush = 5;

// Rank values reserved per category; C(13,3) = 286 is the largest
// count any category needs.
constexpr int kCategoryBudget = 286;

// n choose k, exact in int for the small arguments used here.
constexpr int binomial(int n, int k) {
    if (k < 0 || k > n) return 0;
    int result = 1;
    for (int i = 0; i < k; ++i) {
        result = result * (n - i) / (i + 1);
    }
    return result;
}

// Position of three distinct ascending ranks in colexicographic
// order, so a higher top card always ranks higher.
int colex_index_3(int rank_low, int rank_mid, int rank_high) {
    return binomial(rank_low, 1) + binomial(rank_mid, 2) + binomial(rank_high, 3);
}

// Kicker rank renumbered to 0..11 by skipping the pair's rank.
int compact_rank(int rank, int excluded_rank) {
    return rank < excluded_rank ? rank : rank - 1;
}

// A-2-3 is the lowest straight: rank bits 0, 1 and 12, and it
// ranks as three-high.
constexpr std::uint32_t kWheelBitmask = 0x1003;
constexpr int kWheelStraightHigh = 1;

}

// Returns category * kCategoryBudget + a within-category order, so
// plain integer comparison decides a showdown.
HandRank evaluate_3card(const Card* cards) {
    std::array<int, kRankCount> rank_counts = {};
    std::uint32_t rank_bitmask = 0;
    bool is_flush = true;
    int first_suit = card_suit(cards[0]);
    for (int i = 0; i < 3; ++i) {
        int rank = card_rank(cards[i]);
        ++rank_counts[rank];
        rank_bitmask |= 1u << rank;
        if (card_suit(cards[i]) != first_suit) is_flush = false;
    }

    bool all_distinct_ranks = std::popcount(rank_bitmask) == 3;
    bool is_wheel = rank_bitmask == kWheelBitmask;
    bool is_straight = false;
    int straight_high = -1;
    if (all_distinct_ranks) {
        if (is_wheel) {
            is_straight = true;
            straight_high = kWheelStraightHigh;
        } else {
            // Three distinct ranks are consecutive when the bits
            // span 2.
            int lowest_set_bit = std::countr_zero(rank_bitmask);
            int highest_set_bit = 31 - std::countl_zero(rank_bitmask);
            if (highest_set_bit - lowest_set_bit == 2) {
                is_straight = true;
                straight_high = highest_set_bit;
            }
        }
    }

    if (is_straight && is_flush) {
        return static_cast<HandRank>(kStraightFlush * kCategoryBudget + straight_high);
    }

    int trip_rank = -1;
    int pair_rank = -1;
    std::array<int, 3> single_ranks = {-1, -1, -1};
    int single_count = 0;
    // Ascending scan leaves single_ranks sorted for colex_index_3.
    for (int rank = 0; rank < kRankCount; ++rank) {
        switch (rank_counts[rank]) {
            case 3: trip_rank = rank; break;
            case 2: pair_rank = rank; break;
            case 1: single_ranks[single_count++] = rank; break;
        }
    }

    if (trip_rank != -1) {
        return static_cast<HandRank>(kTrips * kCategoryBudget + trip_rank);
    }
    if (is_straight) {
        return static_cast<HandRank>(kStraight * kCategoryBudget + straight_high);
    }
    if (is_flush) {
        return static_cast<HandRank>(kFlush * kCategoryBudget +
                                      colex_index_3(single_ranks[0], single_ranks[1], single_ranks[2]));
    }
    if (pair_rank != -1) {
        int kicker = compact_rank(single_ranks[0], pair_rank);
        return static_cast<HandRank>(kPair * kCategoryBudget + pair_rank * kRankCount + kicker);
    }
    return static_cast<HandRank>(kHighCard * kCategoryBudget +
                                  colex_index_3(single_ranks[0], single_ranks[1], single_ranks[2]));
}

}
