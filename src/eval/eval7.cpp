#include "eval/eval7.hpp"

#include <algorithm>
#include <array>
#include <cstddef>

#if defined(__AVX2__) && !defined(CFR_FORCE_SCALAR_BATCH)
#include <immintrin.h>
#endif

namespace cfr::eval {

namespace {

constexpr int kCardsPerHand = 7;

// All C(7,5) = 21 ways to choose 5 of 7 card slots, as index quintuples into
// a 7-card hand -- evaluate_7card's whole job is "try every 5-card subset,
// keep the best."
constexpr std::array<std::array<int, 5>, 21> kFiveOfSevenSubsets = {{
    {0, 1, 2, 3, 4}, {0, 1, 2, 3, 5}, {0, 1, 2, 3, 6}, {0, 1, 2, 4, 5}, {0, 1, 2, 4, 6},
    {0, 1, 2, 5, 6}, {0, 1, 3, 4, 5}, {0, 1, 3, 4, 6}, {0, 1, 3, 5, 6}, {0, 1, 4, 5, 6},
    {0, 2, 3, 4, 5}, {0, 2, 3, 4, 6}, {0, 2, 3, 5, 6}, {0, 2, 4, 5, 6}, {0, 3, 4, 5, 6},
    {1, 2, 3, 4, 5}, {1, 2, 3, 4, 6}, {1, 2, 3, 5, 6}, {1, 2, 4, 5, 6}, {1, 3, 4, 5, 6},
    {2, 3, 4, 5, 6},
}};

}  // namespace

HandRank evaluate_7card(const Card* cards) {
    HandRank best = 0;
    for (const auto& subset : kFiveOfSevenSubsets) {
        std::array<Card, 5> five_card_hand = {cards[subset[0]], cards[subset[1]], cards[subset[2]],
                                               cards[subset[3]], cards[subset[4]]};
        best = std::max(best, evaluate_5card(five_card_hand.data()));
    }
    return best;
}

#if defined(__AVX2__) && !defined(CFR_FORCE_SCALAR_BATCH)

// evaluate_7card's per-hand work is a branchy bitmask/histogram computation
// with no natural data-parallel structure across hands (each hand's control
// flow depends on its own cards), so vectorizing the evaluation itself isn't
// a realistic win here. What AVX2 buys cheaply instead is the write-out:
// hands are scored 16 at a time -- a __m256i holds 16 uint16_t lanes -- into
// a local aligned buffer, then flushed with one vector store instead of 16
// scalar writes.
void evaluate_7card_batch(const Card* hands, std::size_t hand_count, HandRank* ranks_out) {
    constexpr std::size_t kLanesPerBlock = 16;
    alignas(32) HandRank block_buffer[kLanesPerBlock];

    std::size_t hand_index = 0;
    for (; hand_index + kLanesPerBlock <= hand_count; hand_index += kLanesPerBlock) {
        for (std::size_t lane = 0; lane < kLanesPerBlock; ++lane) {
            block_buffer[lane] = evaluate_7card(hands + (hand_index + lane) * kCardsPerHand);
        }
        __m256i scored_lanes = _mm256_load_si256(reinterpret_cast<const __m256i*>(block_buffer));
        _mm256_storeu_si256(reinterpret_cast<__m256i*>(ranks_out + hand_index), scored_lanes);
    }
    for (; hand_index < hand_count; ++hand_index) {
        ranks_out[hand_index] = evaluate_7card(hands + hand_index * kCardsPerHand);
    }
}

#else

// No AVX2 available: unroll the scalar loop by 8 so the compiler has an
// independent-iteration window to schedule, without depending on any vector
// ISA. Produces bit-identical results to the AVX2 path above -- V9 pins that
// equivalence, not the mechanism.
void evaluate_7card_batch(const Card* hands, std::size_t hand_count, HandRank* ranks_out) {
    constexpr std::size_t kUnrollFactor = 8;

    std::size_t hand_index = 0;
    for (; hand_index + kUnrollFactor <= hand_count; hand_index += kUnrollFactor) {
        for (std::size_t lane = 0; lane < kUnrollFactor; ++lane) {
            ranks_out[hand_index + lane] = evaluate_7card(hands + (hand_index + lane) * kCardsPerHand);
        }
    }
    for (; hand_index < hand_count; ++hand_index) {
        ranks_out[hand_index] = evaluate_7card(hands + hand_index * kCardsPerHand);
    }
}

#endif

}  // namespace cfr::eval
