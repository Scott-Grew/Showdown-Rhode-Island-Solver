// GameShrink partitions for one Rhode Island Hold'em player.
// compute() reproduces the authors' classes: equal child-label
// multisets, and at a showdown equal opponent-outcome multisets.
// That leaf test never looks at the opponent's class, so their
// partition is lossy in the full game. Canonical labels still
// assign dense ids in first-seen order.

#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <span>
#include <vector>

#include "game/card.hpp"

namespace cfr::abstraction {

// Authors' GameShrink partition, and the lossless refinement of it.
class RihPartition {
public:
    static RihPartition compute();
    // Authors' rule omits the opponent's class, so compute() is lossy
    // in the full game. refined() is the coarsest fixed point of the
    // full condition; compute() stays as the reproduction of theirs.
    RihPartition refined() const;
    // own_cards = {hole}, {hole, flop} or {hole, flop, turn}, our card ids.
    std::uint16_t class_of(std::span<const Card> own_cards) const;
    std::uint32_t class_count(std::size_t round) const;  // round 0, 1, 2

private:
    // opponent_labels is false for the authors' leaf keyed by loss/tie/win.
    RihPartition label(std::span<const std::uint16_t> showdown_rank,
                       bool opponent_labels) const;
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
