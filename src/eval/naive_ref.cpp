#include "eval/naive_ref.hpp"

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

}

NaiveRank naive_evaluate_3card(const Card* cards) {
    std::array<int, kRankCount> rank_counts = {};
    std::array<int, 3> ranks;
    std::array<int, 3> suits;
    for (int i = 0; i < 3; ++i) {
        ranks[i] = card_rank(cards[i]);
        suits[i] = card_suit(cards[i]);
        ++rank_counts[ranks[i]];
    }

    bool is_flush = suits[0] == suits[1] && suits[1] == suits[2];

    std::array<int, 3> sorted_ranks = ranks;
    std::sort(sorted_ranks.begin(), sorted_ranks.end());
    bool all_distinct = sorted_ranks[0] != sorted_ranks[1] && sorted_ranks[1] != sorted_ranks[2];
    bool is_wheel = sorted_ranks == kWheelRanksAscending3;
    bool is_straight = all_distinct && (is_wheel || sorted_ranks[2] - sorted_ranks[0] == 2);
    int straight_high = is_wheel ? sorted_ranks[1] : sorted_ranks[2];

    std::vector<std::pair<int, int>> count_rank_pairs;
    for (int rank = 0; rank < kRankCount; ++rank) {
        if (rank_counts[rank] > 0) count_rank_pairs.emplace_back(rank_counts[rank], rank);
    }
    std::sort(count_rank_pairs.begin(), count_rank_pairs.end(), [](const auto& a, const auto& b) {
        return a.first != b.first ? a.first > b.first : a.second > b.second;
    });

    NaiveRank result;
    for (std::size_t i = 0; i < count_rank_pairs.size(); ++i) {
        result.tiebreak[i] = count_rank_pairs[i].second;
    }

    if (is_straight && is_flush) {
        result.category = 5;
        result.tiebreak = {straight_high, -1, -1, -1, -1};
    } else if (count_rank_pairs[0].first == 3) {
        result.category = 4;
        result.tiebreak = {count_rank_pairs[0].second, -1, -1, -1, -1};
    } else if (is_straight) {
        result.category = 3;
        result.tiebreak = {straight_high, -1, -1, -1, -1};
    } else if (is_flush) {
        result.category = 2;
    } else if (count_rank_pairs[0].first == 2) {
        result.category = 1;
    } else {
        result.category = 0;
    }
    return result;
}

}
