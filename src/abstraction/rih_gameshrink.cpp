// One labelling pass for Rhode Island Hold'em GameShrink.
// compute() runs it from a single class and ignores opponent labels.
// refined() repeats it with those labels until the classes stop splitting.

#include "abstraction/rih_gameshrink.hpp"

#include <algorithm>
#include <array>
#include <cassert>
#include <cstddef>
#include <cstdint>
#include <map>
#include <span>
#include <stdexcept>
#include <utility>
#include <vector>

#include "eval/eval3.hpp"

namespace cfr::abstraction {

namespace {

constexpr std::size_t pair_index(int hole, int flop) {
    return static_cast<std::size_t>(hole) * kCardCount +
           static_cast<std::size_t>(flop);
}

constexpr std::size_t triple_index(int hole, int flop, int turn) {
    return (static_cast<std::size_t>(hole) * kCardCount +
            static_cast<std::size_t>(flop)) *
               kCardCount +
           static_cast<std::size_t>(turn);
}

// find before insert: a round-3 key holds 49 opponent entries, and
// emplace would copy it on every one of the 132k hands.
template <typename Key>
std::uint16_t intern(std::map<Key, std::uint16_t>& ids, const Key& key,
                     std::uint16_t& next_id) {
    auto found = ids.find(key);
    if (found != ids.end()) return found->second;
    ids.emplace(key, next_id);
    return next_id++;
}

template <typename Visit>
void for_each_turn(Visit&& visit) {
    for (int hole = 0; hole < kCardCount; ++hole) {
        for (int flop = 0; flop < kCardCount; ++flop) {
            if (flop == hole) continue;
            for (int turn = 0; turn < kCardCount; ++turn) {
                if (turn == hole || turn == flop) continue;
                visit(hole, flop, turn);
            }
        }
    }
}

constexpr std::size_t kOpponents = static_cast<std::size_t>(kCardCount) - 3;
constexpr std::size_t kTurns = static_cast<std::size_t>(kCardCount) - 2;
constexpr std::size_t kFlops = static_cast<std::size_t>(kCardCount) - 1;
constexpr std::size_t kTriples =
    static_cast<std::size_t>(kCardCount) * kCardCount * kCardCount;
constexpr std::size_t kPairs =
    static_cast<std::size_t>(kCardCount) * kCardCount;

std::vector<std::uint16_t> showdown_ranks() {
    std::vector<std::uint16_t> rank(kTriples, 0);
    std::array<Card, 3> hand{};
    for_each_turn([&](int hole, int flop, int turn) {
        hand[0] = static_cast<Card>(hole);
        hand[1] = static_cast<Card>(flop);
        hand[2] = static_cast<Card>(turn);
        rank[triple_index(hole, flop, turn)] =
            eval::evaluate_3card(hand.data());
    });
    return rank;
}

// Current class plus the sorted multiset of child ids.
template <std::size_t N>
using Bag = std::pair<std::uint16_t, std::array<std::uint16_t, N>>;

template <std::size_t N, typename Fill>
std::uint16_t intern_bag(std::map<Bag<N>, std::uint16_t>& ids,
                         std::uint16_t& next, std::uint16_t current,
                         Fill&& fill) {
    Bag<N> key{};
    key.first = current;
    fill(key.second);
    std::sort(key.second.begin(), key.second.end());
    return intern(ids, key, next);
}

}

// Round 2's (turn) key is the current class plus the sorted (opponent
// class, or 0 under the authors' rule; loss/tie/win) pair per opponent
// card. Rounds 1 and 0 key on the current class plus the sorted ids of
// their children. Final round 1 and 2 ids pair the parent's new class
// with that bottom-up id, so no class spans two parents.
RihPartition RihPartition::label(std::span<const std::uint16_t> showdown_rank,
                                 bool opponent_labels) const {
    using Leaf = std::pair<std::uint16_t, std::uint8_t>;
    using LeafKey = std::pair<std::uint16_t, std::array<Leaf, kOpponents>>;
    using ParentKey = std::pair<std::uint16_t, std::uint16_t>;

    std::vector<std::uint16_t> raw3(kTriples, 0);
    std::vector<std::uint16_t> raw2(kPairs, 0);
    std::map<LeafKey, std::uint16_t> ids3;
    std::uint16_t next3 = 0;
    for_each_turn([&](int hole, int flop, int turn) {
        std::size_t self = triple_index(hole, flop, turn);
        LeafKey key;
        key.first = class_by_tuple_[kRound2Offset + self];
        std::uint16_t mine = showdown_rank[self];
        std::size_t count = 0;
        for (int opponent = 0; opponent < kCardCount; ++opponent) {
            if (opponent == hole || opponent == flop || opponent == turn)
                continue;
            std::size_t opp = triple_index(opponent, flop, turn);
            std::uint16_t theirs = showdown_rank[opp];
            constexpr std::uint8_t kOutcomeLoss = 0;
            constexpr std::uint8_t kOutcomeTie = 1;
            constexpr std::uint8_t kOutcomeWin = 2;
            std::uint8_t outcome = kOutcomeTie;
            if (mine > theirs)
                outcome = kOutcomeWin;
            else if (mine < theirs)
                outcome = kOutcomeLoss;
            std::uint16_t opp_class = 0;
            if (opponent_labels)
                opp_class = class_by_tuple_[kRound2Offset + opp];
            key.second[count++] = {opp_class, outcome};
        }
        std::sort(key.second.begin(), key.second.end());
        raw3[self] = intern(ids3, key, next3);
    });

    std::map<Bag<kTurns>, std::uint16_t> ids2;
    std::uint16_t next2 = 0;
    for (int hole = 0; hole < kCardCount; ++hole) {
        for (int flop = 0; flop < kCardCount; ++flop) {
            if (flop == hole) continue;
            std::size_t index = pair_index(hole, flop);
            raw2[index] = intern_bag<kTurns>(
                ids2, next2, class_by_tuple_[kRound1Offset + index],
                [&](std::array<std::uint16_t, kTurns>& children) {
                    std::size_t count = 0;
                    for (int turn = 0; turn < kCardCount; ++turn) {
                        if (turn == hole || turn == flop) continue;
                        children[count++] =
                            raw3[triple_index(hole, flop, turn)];
                    }
                });
        }
    }

    RihPartition next;
    next.class_by_tuple_.assign(kStorageSize, 0);
    std::map<Bag<kFlops>, std::uint16_t> ids1;
    std::uint16_t next1 = 0;
    for (int hole = 0; hole < kCardCount; ++hole) {
        std::size_t hole_index = static_cast<std::size_t>(hole);
        next.class_by_tuple_[kRound0Offset + hole_index] = intern_bag<kFlops>(
            ids1, next1, class_by_tuple_[kRound0Offset + hole_index],
            [&](std::array<std::uint16_t, kFlops>& children) {
                std::size_t count = 0;
                for (int flop = 0; flop < kCardCount; ++flop) {
                    if (flop == hole) continue;
                    children[count++] = raw2[pair_index(hole, flop)];
                }
            });
    }
    next.class_counts_[0] = next1;

    std::map<ParentKey, std::uint16_t> parent2;
    std::uint16_t next_class2 = 0;
    for (int hole = 0; hole < kCardCount; ++hole) {
        std::uint16_t parent =
            next.class_by_tuple_[kRound0Offset +
                                 static_cast<std::size_t>(hole)];
        for (int flop = 0; flop < kCardCount; ++flop) {
            if (flop == hole) continue;
            std::size_t index = pair_index(hole, flop);
            next.class_by_tuple_[kRound1Offset + index] =
                intern(parent2, ParentKey{parent, raw2[index]}, next_class2);
        }
    }
    next.class_counts_[1] = next_class2;

    std::map<ParentKey, std::uint16_t> parent3;
    std::uint16_t next_class3 = 0;
    for_each_turn([&](int hole, int flop, int turn) {
        std::size_t index = triple_index(hole, flop, turn);
        std::uint16_t parent =
            next.class_by_tuple_[kRound1Offset + pair_index(hole, flop)];
        next.class_by_tuple_[kRound2Offset + index] =
            intern(parent3, ParentKey{parent, raw3[index]}, next_class3);
    });
    next.class_counts_[2] = next_class3;
    return next;
}

RihPartition RihPartition::compute() {
    RihPartition trivial;
    trivial.class_by_tuple_.assign(kStorageSize, 0);
    return trivial.label(showdown_ranks(), false);
}

RihPartition RihPartition::refined() const {
    const std::vector<std::uint16_t> rank = showdown_ranks();
    RihPartition current = *this;
    // Converges in 2 passes on the authors' partition; 32 is headroom.
    for (int pass = 0; pass < 32; ++pass) {
        RihPartition next = current.label(rank, true);
        if (next.class_counts_ == current.class_counts_) return next;
        current = std::move(next);
    }
    // Cannot happen: a pass only splits, so the counts reach a fixed point.
    throw std::logic_error("RihPartition::refined did not converge");
}

std::uint16_t RihPartition::class_of(std::span<const Card> own_cards) const {
    assert(own_cards.size() == 1 || own_cards.size() == 2 ||
           own_cards.size() == 3);
    switch (own_cards.size()) {
        case 1: return class_by_tuple_[kRound0Offset + own_cards[0]];
        case 2:
            return class_by_tuple_[kRound1Offset +
                                   pair_index(own_cards[0], own_cards[1])];
        default:
            return class_by_tuple_[kRound2Offset + triple_index(own_cards[0],
                                                                own_cards[1],
                                                                own_cards[2])];
    }
}

std::uint32_t RihPartition::class_count(std::size_t round) const {
    assert(round < class_counts_.size());
    return class_counts_[round];
}

}
