// A second, independently written three-card evaluator that
// test_eval3.cpp compares eval3 against.

#pragma once

#include <array>

#include "game/card.hpp"

namespace cfr::eval::naive {

// Categories, weakest first, as eval3 orders them.
constexpr int kHighCard = 0;
constexpr int kPair = 1;
constexpr int kFlush = 2;
constexpr int kStraight = 3;
constexpr int kThreeOfAKind = 4;
constexpr int kStraightFlush = 5;

struct NaiveRank {
    int category = 0;
    std::array<int, 3> tiebreak = {-1, -1, -1};

    bool operator==(const NaiveRank& other) const {
        return category == other.category && tiebreak == other.tiebreak;
    }
    bool operator<(const NaiveRank& other) const {
        if (category != other.category) return category < other.category;
        return tiebreak < other.tiebreak;
    }
};

NaiveRank naive_evaluate_3card(const Card* cards);

}
