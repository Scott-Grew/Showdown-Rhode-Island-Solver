// Card ids are rank-major: id = rank * 4 + suit, onto 0..51.

#include <catch2/catch_test_macros.hpp>

#include "game/card.hpp"

using cfr::Card;
using cfr::card_rank;
using cfr::card_suit;
using cfr::kCardCount;
using cfr::kRankCount;
using cfr::kSuitCount;
using cfr::make_card;

TEST_CASE("card ids are rank-major onto 0..51") {
    // 46 is jack of suit 2 only in rank-major order, not suit-major.
    REQUIRE(card_rank(0) == 0);
    REQUIRE(card_suit(0) == 0);
    REQUIRE(card_rank(46) == 11);
    REQUIRE(card_suit(46) == 2);
    REQUIRE(card_rank(51) == 12);
    REQUIRE(card_suit(51) == 3);

    bool seen[kCardCount] = {};
    for (int rank = 0; rank < kRankCount; ++rank) {
        for (int suit = 0; suit < kSuitCount; ++suit) {
            Card card = make_card(rank, suit);
            REQUIRE(card_rank(card) == rank);
            REQUIRE(card_suit(card) == suit);
            REQUIRE(card < kCardCount);
            REQUIRE_FALSE(seen[card]);
            seen[card] = true;
        }
    }
}
