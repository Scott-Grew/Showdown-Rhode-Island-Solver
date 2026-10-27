// Bottom-up type construction and top-down class construction for
// Rhode Island Hold'em GameShrink.

#include "abstraction/rih_gameshrink.hpp"

#include <algorithm>
#include <array>
#include <cassert>
#include <cstddef>
#include <cstdint>
#include <map>
#include <span>
#include <utility>
#include <vector>

#include "eval/eval3.hpp"

namespace cfr::abstraction {

namespace {

using Type3 = std::array<std::uint8_t, 3>;
using Type2 = std::array<Type3, kCardCount - 2>;
using Type1 = std::array<std::uint16_t, kCardCount - 1>;

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

template <typename Key>
std::uint16_t intern_label(std::map<Key, std::uint16_t>& ids, const Key& key,
                           std::uint16_t& next_id) {
    auto [entry, inserted] = ids.emplace(key, next_id);
    if (inserted) ++next_id;
    return entry->second;
}

void compute_type3_labels(std::vector<Type3>& type3_by_triple) {
    std::array<eval::HandRank, kCardCount> hole_rank{};
    std::array<int, kCardCount - 2> holes_by_rank{};
    std::array<Card, 3> hand{};

    for (int flop = 0; flop < kCardCount; ++flop) {
        hand[1] = static_cast<Card>(flop);
        for (int turn = 0; turn < kCardCount; ++turn) {
            if (turn == flop) continue;
            hand[2] = static_cast<Card>(turn);

            std::size_t hole_count = 0;
            for (int hole = 0; hole < kCardCount; ++hole) {
                if (hole == flop || hole == turn) continue;
                holes_by_rank[hole_count++] = hole;
                hand[0] = static_cast<Card>(hole);
                hole_rank[static_cast<std::size_t>(hole)] =
                    eval::evaluate_3card(hand.data());
            }

            std::sort(holes_by_rank.begin(), holes_by_rank.end(),
                      [&hole_rank](int left, int right) {
                          return hole_rank[static_cast<std::size_t>(left)] <
                                 hole_rank[static_cast<std::size_t>(right)];
                      });

            std::size_t begin = 0;
            while (begin < holes_by_rank.size()) {
                std::size_t end = begin + 1;
                eval::HandRank rank =
                    hole_rank[static_cast<std::size_t>(holes_by_rank[begin])];
                while (
                    end < holes_by_rank.size() &&
                    hole_rank[static_cast<std::size_t>(holes_by_rank[end])] ==
                        rank) {
                    ++end;
                }

                std::uint8_t wins = static_cast<std::uint8_t>(begin);
                std::uint8_t ties = static_cast<std::uint8_t>(end - begin - 1);
                std::uint8_t losses =
                    static_cast<std::uint8_t>(holes_by_rank.size() - end);
                for (std::size_t i = begin; i < end; ++i) {
                    int hole = holes_by_rank[i];
                    type3_by_triple[triple_index(hole, flop, turn)] = {
                        wins, ties, losses};
                }
                begin = end;
            }
        }
    }
}

}

RihPartition RihPartition::compute() {
    RihPartition partition;
    partition.class_by_tuple_.assign(kStorageSize, 0);

    std::vector<Type3> type3_by_triple(static_cast<std::size_t>(kCardCount) *
                                       kCardCount * kCardCount);
    std::vector<std::uint16_t> type2_by_pair(
        static_cast<std::size_t>(kCardCount) * kCardCount, 0);

    compute_type3_labels(type3_by_triple);

    std::map<Type2, std::uint16_t> type2_ids;
    std::uint16_t next_type2_id = 0;
    for (int hole = 0; hole < kCardCount; ++hole) {
        for (int flop = 0; flop < kCardCount; ++flop) {
            if (flop == hole) continue;
            Type2 type2{};
            std::size_t turn_index = 0;
            for (int turn = 0; turn < kCardCount; ++turn) {
                if (turn == hole || turn == flop) continue;
                type2[turn_index++] =
                    type3_by_triple[triple_index(hole, flop, turn)];
            }
            std::sort(type2.begin(), type2.end());
            type2_by_pair[pair_index(hole, flop)] =
                intern_label(type2_ids, type2, next_type2_id);
        }
    }

    std::map<Type1, std::uint16_t> class1_ids;
    std::uint16_t next_class1_id = 0;
    for (int hole = 0; hole < kCardCount; ++hole) {
        Type1 type1{};
        std::size_t flop_index = 0;
        for (int flop = 0; flop < kCardCount; ++flop) {
            if (flop == hole) continue;
            type1[flop_index++] = type2_by_pair[pair_index(hole, flop)];
        }
        std::sort(type1.begin(), type1.end());
        partition
            .class_by_tuple_[kRound0Offset + static_cast<std::size_t>(hole)] =
            intern_label(class1_ids, type1, next_class1_id);
    }
    partition.class_counts_[0] = next_class1_id;

    using Class2Key = std::pair<std::uint16_t, std::uint16_t>;
    std::map<Class2Key, std::uint16_t> class2_ids;
    std::uint16_t next_class2_id = 0;
    for (int hole = 0; hole < kCardCount; ++hole) {
        std::uint16_t parent =
            partition.class_by_tuple_[kRound0Offset +
                                      static_cast<std::size_t>(hole)];
        for (int flop = 0; flop < kCardCount; ++flop) {
            if (flop == hole) continue;
            std::size_t index = pair_index(hole, flop);
            Class2Key key{parent, type2_by_pair[index]};
            partition.class_by_tuple_[kRound1Offset + index] =
                intern_label(class2_ids, key, next_class2_id);
        }
    }
    partition.class_counts_[1] = next_class2_id;

    using Class3Key = std::pair<std::uint16_t, Type3>;
    std::map<Class3Key, std::uint16_t> class3_ids;
    std::uint16_t next_class3_id = 0;
    for (int hole = 0; hole < kCardCount; ++hole) {
        for (int flop = 0; flop < kCardCount; ++flop) {
            if (flop == hole) continue;
            std::uint16_t parent =
                partition
                    .class_by_tuple_[kRound1Offset + pair_index(hole, flop)];
            for (int turn = 0; turn < kCardCount; ++turn) {
                if (turn == hole || turn == flop) continue;
                std::size_t index = triple_index(hole, flop, turn);
                Class3Key key{parent, type3_by_triple[index]};
                partition.class_by_tuple_[kRound2Offset + index] =
                    intern_label(class3_ids, key, next_class3_id);
            }
        }
    }
    partition.class_counts_[2] = next_class3_id;

    return partition;
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
