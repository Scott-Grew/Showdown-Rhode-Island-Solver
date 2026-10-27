// Three-card hand evaluator: packs a category and an order within the
// category into one comparable integer.

#include "eval/eval3.hpp"

#include <algorithm>
#include <array>
#include <bit>
#include <cstdint>

namespace cfr::eval {

namespace {

using cfr::card_rank;
using cfr::card_suit;
using cfr::kRankCount;

// Hand categories, weakest first. Straights
// outrank flushes in three-card poker.
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

// Position of three distinct ascending values in colexicographic
// order. Ranks and card ids both use it.
constexpr int colex_index_3(int rank_low, int rank_mid, int rank_high) {
    return binomial(rank_low, 1) + binomial(rank_mid, 2) +
           binomial(rank_high, 3);
}

// C(52, 3). The last three card ids are the highest index.
static_assert(colex_index_3(49, 50, 51) == 22099);
constexpr std::size_t kThreeCardHands = 22100;

// Kicker rank renumbered to 0..11 by skipping the pair's rank.
int compact_rank(int rank, int excluded_rank) {
    return rank < excluded_rank ? rank : rank - 1;
}

// A-2-3 is the lowest straight: rank bits 0, 1 and 12, and it
// ranks as three-high.
constexpr std::uint32_t kWheelBitmask = 0x1003;
constexpr int kWheelStraightHigh = 1;

// Marks the absence of a straight, a pair or trips.
constexpr int kNoRank = -1;

// High rank of the straight made by the ranks in the bitmask, or
// kNoRank when they do not form one.
int straight_high_rank(std::uint32_t rank_bitmask) {
    if (std::popcount(rank_bitmask) != 3) return kNoRank;
    if (rank_bitmask == kWheelBitmask) return kWheelStraightHigh;
    // Three distinct ranks are consecutive when the bits span 2.
    int lowest_set_bit = std::countr_zero(rank_bitmask);
    int highest_set_bit = 31 - std::countl_zero(rank_bitmask);
    if (highest_set_bit - lowest_set_bit != 2) return kNoRank;
    return highest_set_bit;
}

// The ranks of a hand split by how often each occurs. Unpaired
// ranks are ascending, as colex_index_3 needs.
struct RankGroups {
    int trip_rank = kNoRank;
    int pair_rank = kNoRank;
    std::array<int, 3> single_ranks = {kNoRank, kNoRank, kNoRank};
};

RankGroups group_ranks(const std::array<int, kRankCount>& rank_counts) {
    RankGroups groups;
    std::size_t single_count = 0;
    for (int rank = 0; rank < kRankCount; ++rank) {
        std::size_t rank_index = static_cast<std::size_t>(rank);
        switch (rank_counts[rank_index]) {
            case 3: groups.trip_rank = rank; break;
            case 2: groups.pair_rank = rank; break;
            case 1: groups.single_ranks[single_count++] = rank; break;
        }
    }
    return groups;
}

HandRank hand_rank(int category, int order_within_category) {
    return static_cast<HandRank>(category * kCategoryBudget +
                                 order_within_category);
}
}

// category * kCategoryBudget + order, so integer comparison is the showdown.
HandRank evaluate_3card(const Card* cards) {
    std::array<int, kRankCount> rank_counts = {};
    std::uint32_t rank_bitmask = 0;
    bool is_flush = true;
    int first_suit = card_suit(cards[0]);
    for (int i = 0; i < 3; ++i) {
        int rank = card_rank(cards[i]);
        ++rank_counts[static_cast<std::size_t>(rank)];
        rank_bitmask |= 1u << rank;
        if (card_suit(cards[i]) != first_suit) is_flush = false;
    }

    int straight_high = straight_high_rank(rank_bitmask);
    bool is_straight = straight_high != kNoRank;
    if (is_straight && is_flush)
        return hand_rank(kStraightFlush, straight_high);

    RankGroups groups = group_ranks(rank_counts);
    if (groups.trip_rank != kNoRank) return hand_rank(kTrips, groups.trip_rank);
    if (is_straight) return hand_rank(kStraight, straight_high);

    if (groups.pair_rank != kNoRank) {
        int kicker = compact_rank(groups.single_ranks[0], groups.pair_rank);
        return hand_rank(kPair, groups.pair_rank * kRankCount + kicker);
    }

    // A flush cannot hold a pair, so both remaining categories have
    // three unpaired ranks.
    int unpaired_order = colex_index_3(
        groups.single_ranks[0], groups.single_ranks[1], groups.single_ranks[2]);
    return hand_rank(is_flush ? kFlush : kHighCard, unpaired_order);
}

namespace {

// Filled by evaluate_3card, so the table cannot drift from it.
const std::array<HandRank, kThreeCardHands>& three_card_table() {
    static const std::array<HandRank, kThreeCardHands> table = [] {
        std::array<HandRank, kThreeCardHands> ranks{};
        Card hand[3];
        for (int low = 0; low < kCardCount; ++low) {
            for (int mid = low + 1; mid < kCardCount; ++mid) {
                for (int high = mid + 1; high < kCardCount; ++high) {
                    hand[0] = static_cast<Card>(low);
                    hand[1] = static_cast<Card>(mid);
                    hand[2] = static_cast<Card>(high);
                    ranks[static_cast<std::size_t>(
                        colex_index_3(low, mid, high))] = evaluate_3card(hand);
                }
            }
        }
        return ranks;
    }();
    return table;
}

}

HandRank lookup_3card(const Card* cards) {
    int low = cards[0];
    int mid = cards[1];
    int high = cards[2];
    if (low > mid) std::swap(low, mid);
    if (mid > high) std::swap(mid, high);
    if (low > mid) std::swap(low, mid);
    return three_card_table()[static_cast<std::size_t>(
        colex_index_3(low, mid, high))];
}

}
