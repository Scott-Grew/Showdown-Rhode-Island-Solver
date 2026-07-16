#pragma once

#include <array>

#include "game/card.hpp"

namespace cfr::eval::naive {

// Obviously-correct, deliberately unoptimized reference evaluator: classify
// by counting rank frequencies (no bitmasks, no combinatorial packing, no
// shared code with eval5.cpp's fast path), then compare hands lexicographically
// by (category, tiebreak). Test-only -- never linked into cfr_eval consumers.
//
// category: 0 (high card) .. 8 (straight flush), same ordinal scale the fast
// path uses internally, though the two are never compared directly -- only
// their induced orderings are checked against each other.
//
// tiebreak: the hand's significant ranks in the order a human would compare
// them -- sorted by count descending, then rank descending -- padded with -1
// out to 5 entries. Straights (and straight flushes) override this to a
// single high-card digit, since ace-low (wheel) plays as the lowest straight
// rather than the highest card.
struct NaiveRank {
    int category = 0;
    std::array<int, 5> tiebreak = {-1, -1, -1, -1, -1};

    bool operator==(const NaiveRank& other) const {
        return category == other.category && tiebreak == other.tiebreak;
    }
    bool operator<(const NaiveRank& other) const {
        if (category != other.category) return category < other.category;
        return tiebreak < other.tiebreak;  // std::array<int, 5>::operator< is lexicographic
    }
};

NaiveRank naive_evaluate_5card(const Card* cards);

}  // namespace cfr::eval::naive
