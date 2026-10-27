// Independent three-card evaluator: ranks are counted and sorted,
// not packed the way eval3 packs them.

#include "naive_ref.hpp"

#include <algorithm>
#include <array>
#include <utility>
#include <vector>

namespace cfr::eval::naive {

namespace {

using cfr::card_rank;
using cfr::card_suit;
using cfr::kRankCount;

constexpr std::array<int, 3> kWheelRanksAscending3 = {0, 1, 12};

struct Classified {
    bool is_flush = false;
    bool is_straight = false;
    int straight_high = 0;
    std::vector<std::pair<int, int>> count_rank_pairs;
};

Classified classify(const Card* cards) {
    std::array<int, kRankCount> rank_counts = {};
    std::array<int, 3> ranks;
    std::array<int, 3> suits;
    for (int i = 0; i < 3; ++i) {
        std::size_t card_index = static_cast<std::size_t>(i);
        ranks[card_index] = card_rank(cards[card_index]);
        suits[card_index] = card_suit(cards[card_index]);
        ++rank_counts[static_cast<std::size_t>(ranks[card_index])];
    }

    Classified hand;
    hand.is_flush = suits[0] == suits[1] && suits[1] == suits[2];
    std::array<int, 3> sorted_ranks = ranks;
    std::sort(sorted_ranks.begin(), sorted_ranks.end());
    bool all_distinct = sorted_ranks[0] != sorted_ranks[1] &&
                        sorted_ranks[1] != sorted_ranks[2];
    bool is_wheel = sorted_ranks == kWheelRanksAscending3;
    hand.is_straight =
        all_distinct && (is_wheel || sorted_ranks[2] - sorted_ranks[0] == 2);
    // A-2-3 ranks as three-high, below 2-3-4.
    hand.straight_high = is_wheel ? sorted_ranks[1] : sorted_ranks[2];

    for (int rank = 0; rank < kRankCount; ++rank) {
        std::size_t rank_index = static_cast<std::size_t>(rank);
        if (rank_counts[rank_index] > 0)
            hand.count_rank_pairs.emplace_back(rank_counts[rank_index], rank);
    }
    std::sort(hand.count_rank_pairs.begin(), hand.count_rank_pairs.end(),
              [](const auto& a, const auto& b) {
                  return a.first != b.first ? a.first > b.first
                                            : a.second > b.second;
              });
    return hand;
}

}

NaiveRank naive_evaluate_3card(const Card* cards) {
    Classified hand = classify(cards);
    NaiveRank result;
    for (std::size_t i = 0; i < hand.count_rank_pairs.size(); ++i)
        result.tiebreak[i] = hand.count_rank_pairs[i].second;

    if (hand.is_straight && hand.is_flush) {
        result.category = 5;
        result.tiebreak = {hand.straight_high, -1, -1, -1, -1};
    } else if (hand.count_rank_pairs[0].first == 3) {
        result.category = 4;
        result.tiebreak = {hand.count_rank_pairs[0].second, -1, -1, -1, -1};
    } else if (hand.is_straight) {
        result.category = 3;
        result.tiebreak = {hand.straight_high, -1, -1, -1, -1};
    } else if (hand.is_flush) {
        result.category = 2;
    } else if (hand.count_rank_pairs[0].first == 2) {
        result.category = 1;
    } else {
        result.category = 0;
    }
    return result;
}

}
