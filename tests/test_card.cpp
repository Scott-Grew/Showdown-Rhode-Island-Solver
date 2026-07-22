#include <catch2/catch_test_macros.hpp>

#include "game/card.hpp"
#include "game/game.hpp"

using cfr::Card;
using cfr::card_rank;
using cfr::card_suit;
using cfr::kCardCount;
using cfr::kRankCount;
using cfr::kSuitCount;
using cfr::make_card;

TEST_CASE("card: encode/decode roundtrip for all 52 cards") {
    for (int rank = 0; rank < kRankCount; ++rank) {
        for (int suit = 0; suit < kSuitCount; ++suit) {
            Card card = make_card(rank, suit);
            REQUIRE(card_rank(card) == rank);
            REQUIRE(card_suit(card) == suit);
        }
    }
}

TEST_CASE("card: every encoding 0..51 is reachable and unique") {
    bool seen[kCardCount] = {};
    for (int rank = 0; rank < kRankCount; ++rank) {
        for (int suit = 0; suit < kSuitCount; ++suit) {
            Card card = make_card(rank, suit);
            REQUIRE(card < kCardCount);
            REQUIRE_FALSE(seen[card]);
            seen[card] = true;
        }
    }
}

TEST_CASE("card: directed rank/suit extraction") {

    REQUIRE(card_rank(0) == 0);
    REQUIRE(card_suit(0) == 0);

    REQUIRE(card_rank(51) == 12);
    REQUIRE(card_suit(51) == 3);

    REQUIRE(card_rank(46) == 11);
    REQUIRE(card_suit(46) == 2);
}
