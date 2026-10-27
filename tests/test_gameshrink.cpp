#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <array>
#include <cstdint>
#include <map>
#include <span>
#include <unordered_map>
#include <utility>
#include <vector>

#include "abstraction/rih_gameshrink.hpp"
#include "eval/eval3.hpp"
#include "game/card.hpp"
#include "grader/gsi_strategy.hpp"

using cfr::Card;
using cfr::kCardCount;
using cfr::abstraction::RihPartition;
using cfr::grader::GsiStrategy;

namespace {
void check(const GsiStrategy& strategy, std::size_t round) {
    static const RihPartition part = RihPartition::compute();
    std::vector<int> fwd(part.class_count(round), -1);
    std::unordered_map<std::uint16_t, std::uint16_t> rev;
    auto same = [&](std::uint16_t ours, std::uint16_t theirs) {
        if (fwd[ours] >= 0) REQUIRE(fwd[ours] == theirs);
        if (fwd[ours] < 0) fwd[ours] = theirs;
        auto [it, ok] = rev.emplace(theirs, ours);
        if (!ok) REQUIRE(it->second == ours);
    };
    std::array<Card, 3> c{};
    auto cards = [&](std::size_t n) {
        return std::span<const Card>(c.data(), n);
    };
    for (int h = 0; h < kCardCount; ++h) {
        c[0] = static_cast<Card>(h);
        if (round == 0)
            same(part.class_of(cards(1)), strategy.signal_class(cards(1)));
        for (int f = 0; f < kCardCount && round > 0; ++f) {
            if (f == h) continue;
            c[1] = static_cast<Card>(f);
            if (round == 1)
                same(part.class_of(cards(2)), strategy.signal_class(cards(2)));
            for (int t = 0; t < kCardCount && round > 1; ++t) {
                if (t == h || t == f) continue;
                c[2] = static_cast<Card>(t);
                same(part.class_of(cards(3)), strategy.signal_class(cards(3)));
            }
        }
    }
    for (int id : fwd) REQUIRE(id >= 0);
    REQUIRE(rev.size() == fwd.size());
}

}

TEST_CASE("partition class counts match the published integers") {
    const RihPartition authors = RihPartition::compute();
    const RihPartition refined = authors.refined();
    REQUIRE(authors.class_count(0) == 13);
    REQUIRE(authors.class_count(1) == 205);
    REQUIRE(authors.class_count(2) == 1774);
    REQUIRE(refined.class_count(0) == 13);
    REQUIRE(refined.class_count(1) == 325);
    REQUIRE(refined.class_count(2) == 7969);
}

TEST_CASE("gameshrink partition is equivalent to the published partition") {
    GsiStrategy strategy = GsiStrategy::load(CFR_GSI_DATA_DIR);
    INFO("requires GSI data: run make data/gsi/partition");
    REQUIRE(strategy.available());
    check(strategy, 0);
    check(strategy, 1);
    check(strategy, 2);
}

namespace {

// Suits renumbered in the order they first appear in the hand.
std::array<int, 3> suit_canonical(std::span<const int> cards) {
    std::array<int, 4> suit_id{-1, -1, -1, -1};
    int next = 0;
    std::array<int, 3> out{};
    for (std::size_t i = 0; i < cards.size(); ++i) {
        auto card = static_cast<Card>(cards[i]);
        int suit = cfr::card_suit(card);
        if (suit_id[static_cast<std::size_t>(suit)] < 0)
            suit_id[static_cast<std::size_t>(suit)] = next++;
        out[i] =
            cfr::card_rank(card) * 4 + suit_id[static_cast<std::size_t>(suit)];
    }
    return out;
}

std::uint8_t showdown(int hole, int opponent, int flop, int turn) {
    std::array<Card, 3> mine{static_cast<Card>(hole), static_cast<Card>(flop),
                             static_cast<Card>(turn)};
    std::array<Card, 3> theirs{static_cast<Card>(opponent),
                               static_cast<Card>(flop),
                               static_cast<Card>(turn)};
    cfr::eval::HandRank mine_rank = cfr::eval::evaluate_3card(mine.data());
    cfr::eval::HandRank their_rank = cfr::eval::evaluate_3card(theirs.data());
    if (mine_rank > their_rank) return 2;
    if (mine_rank < their_rank) return 0;
    return 1;
}

}

TEST_CASE("refined partition refines gameshrink and respects suits") {
    const RihPartition authors = RihPartition::compute();
    const RihPartition part = authors.refined();
    std::array<std::vector<int>, 3> author_of{
        std::vector<int>(part.class_count(0), -1),
        std::vector<int>(part.class_count(1), -1),
        std::vector<int>(part.class_count(2), -1)};
    std::array<std::map<std::array<int, 3>, std::uint16_t>, 3> suit_of;
    int refine_miss = 0;
    int suit_miss = 0;
    std::array<Card, 3> cards{};
    auto note = [&](std::size_t round, std::size_t n) {
        auto hand = std::span<const Card>(cards.data(), n);
        std::uint16_t refined = part.class_of(hand);
        std::uint16_t author = authors.class_of(hand);
        int& stored = author_of[round][refined];
        if (stored >= 0 && stored != author) ++refine_miss;
        if (stored < 0) stored = author;
        std::array<int, 3> raw{};
        for (std::size_t i = 0; i < n; ++i) raw[i] = cards[i];
        std::array<int, 3> canon =
            suit_canonical(std::span<const int>(raw.data(), n));
        auto [it, inserted] = suit_of[round].emplace(canon, refined);
        if (!inserted && it->second != refined) ++suit_miss;
    };
    for (int hole = 0; hole < kCardCount; ++hole) {
        cards[0] = static_cast<Card>(hole);
        note(0, 1);
        for (int flop = 0; flop < kCardCount; ++flop) {
            if (flop == hole) continue;
            cards[1] = static_cast<Card>(flop);
            note(1, 2);
            for (int turn = 0; turn < kCardCount; ++turn) {
                if (turn == hole || turn == flop) continue;
                cards[2] = static_cast<Card>(turn);
                note(2, 3);
            }
        }
    }
    REQUIRE(refine_miss == 0);
    REQUIRE(suit_miss == 0);
    for (const std::vector<int>& ids : author_of)
        for (int author : ids) REQUIRE(author >= 0);
}

TEST_CASE("refined round 3 classes share opponent outcome multisets") {
    const RihPartition part = RihPartition::compute().refined();
    using Sig = std::vector<std::pair<std::uint16_t, std::uint8_t>>;
    std::vector<Sig> seen(part.class_count(2));
    std::vector<char> have(part.class_count(2), 0);
    int mismatches = 0;
    std::array<Card, 3> cards{};
    for (int hole = 0; hole < kCardCount; ++hole) {
        for (int flop = 0; flop < kCardCount; ++flop) {
            if (flop == hole) continue;
            for (int turn = 0; turn < kCardCount; ++turn) {
                if (turn == hole || turn == flop) continue;
                cards = {static_cast<Card>(hole), static_cast<Card>(flop),
                         static_cast<Card>(turn)};
                std::uint16_t id = part.class_of(cards);
                Sig sig;
                for (int opponent = 0; opponent < kCardCount; ++opponent) {
                    if (opponent == hole || opponent == flop ||
                        opponent == turn) {
                        continue;
                    }
                    std::array<Card, 3> opp{static_cast<Card>(opponent),
                                            static_cast<Card>(flop),
                                            static_cast<Card>(turn)};
                    sig.push_back({part.class_of(opp),
                                   showdown(hole, opponent, flop, turn)});
                }
                std::sort(sig.begin(), sig.end());
                if (have[id] == 0) {
                    seen[id] = std::move(sig);
                    have[id] = 1;
                } else if (seen[id] != sig) {
                    ++mismatches;
                }
            }
        }
    }
    REQUIRE(mismatches == 0);
    for (char bit : have) REQUIRE(bit != 0);
}
