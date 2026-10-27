// Lossless GameShrink partition for one Rhode Island Hold'em player.
// Ordered game isomorphism is an equivalence relation, so the bipartite
// graph of Algorithm 1 step 3 is a disjoint union of complete bipartite
// blocks, one per class. It has a perfect matching iff each class occurs
// equally often on both sides, i.e. iff the sorted multisets of child
// labels are equal (each child of a node is equally likely, and siblings
// have equally many children). At the leaves, equal payoff over every
// betting history reduces to an equal showdown, and matching over the
// opponent's cards reduces to equal (W, T, L) counts. Canonical labels
// (AHU-style tree isomorphism) therefore compute the same relation by
// sorting instead of O(|Theta|^3) matchings.

#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <span>
#include <vector>

#include "game/card.hpp"

namespace cfr::abstraction {

// Lossless GameShrink partition of one player's own information.
class RihPartition {
public:
    static RihPartition compute();
    // own_cards = {hole}, {hole, flop} or {hole, flop, turn}, our card ids.
    std::uint16_t class_of(std::span<const Card> own_cards) const;
    std::uint32_t class_count(std::size_t round) const;  // round 0, 1, 2

private:
    static constexpr std::size_t kRound0Offset = 0;
    static constexpr std::size_t kRound1Offset = kCardCount;
    static constexpr std::size_t kRound2Offset =
        kRound1Offset + static_cast<std::size_t>(kCardCount) * kCardCount;
    static constexpr std::size_t kStorageSize =
        kRound2Offset +
        static_cast<std::size_t>(kCardCount) * kCardCount * kCardCount;

    std::vector<std::uint16_t> class_by_tuple_;
    std::array<std::uint32_t, 3> class_counts_ = {0, 0, 0};
};

}
