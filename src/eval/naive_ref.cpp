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

constexpr std::array<int, 5> kWheelRanksAscending = {0, 1, 2, 3, 12};  // A-2-3-4-5, ace plays low

}  // namespace

NaiveRank naive_evaluate_5card(const Card* cards) {
    std::array<int, kRankCount> rank_counts = {};
    std::array<int, 5> ranks;
    std::array<int, 5> suits;
    for (int i = 0; i < 5; ++i) {
        ranks[i] = card_rank(cards[i]);
        suits[i] = card_suit(cards[i]);
        ++rank_counts[ranks[i]];
    }

    bool is_flush = std::all_of(suits.begin() + 1, suits.end(), [&](int suit) { return suit == suits[0]; });

    std::array<int, 5> sorted_ranks = ranks;
    std::sort(sorted_ranks.begin(), sorted_ranks.end());
    bool all_distinct = std::adjacent_find(sorted_ranks.begin(), sorted_ranks.end()) == sorted_ranks.end();
    bool is_wheel = sorted_ranks == kWheelRanksAscending;
    bool is_straight = all_distinct && (is_wheel || sorted_ranks.back() - sorted_ranks.front() == 4);
    int straight_high = is_wheel ? 3 : sorted_ranks.back();  // wheel plays the 5 as its top card

    // (count, rank) for every rank present, sorted by count descending then
    // rank descending -- this sequence's count pattern (e.g. [3, 2]) directly
    // names the category, and the sequence itself is the tiebreak digit
    // order a human would compare by hand.
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
        result.category = 8;
        result.tiebreak = {straight_high, -1, -1, -1, -1};
    } else if (count_rank_pairs[0].first == 4) {
        result.category = 7;
    } else if (count_rank_pairs[0].first == 3 && count_rank_pairs[1].first == 2) {
        result.category = 6;
    } else if (is_flush) {
        result.category = 5;
    } else if (is_straight) {
        result.category = 4;
        result.tiebreak = {straight_high, -1, -1, -1, -1};
    } else if (count_rank_pairs[0].first == 3) {
        result.category = 3;
    } else if (count_rank_pairs[1].first == 2) {
        result.category = 2;
    } else if (count_rank_pairs[0].first == 2) {
        result.category = 1;
    } else {
        result.category = 0;
    }
    return result;
}

}  // namespace cfr::eval::naive
