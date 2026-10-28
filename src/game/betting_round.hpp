// Fixed-limit betting shared by Leduc and Rhode Island: legal actions,
// chip contributions and payoffs. Kuhn codes its own shorter betting.

#pragma once

#include <algorithm>
#include <array>
#include <bit>
#include <cassert>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <optional>

#include "game/game.hpp"

namespace cfr::game {

constexpr Action kActionFold = 0;
constexpr Action kActionCallCheck = 1;
constexpr Action kActionRaise = 2;

// A history entry of kChanceCardOffset + card is a dealt card; any
// entry below the offset is a betting action.
constexpr Action kChanceCardOffset = 100;
static_assert(
    kChanceCardOffset + static_cast<Action>(kMaxChanceOutcomes) - 1 <=
        std::numeric_limits<decltype(State::history)::value_type>::max(),
    "State::history must hold every chance action");

constexpr int round_progress_count(int max_raises) {
    return 2 * max_raises + 2;
}

// Check-check through the capped re-raise is max_raises + 1 codes,
// and the lines that opened with a bet add max_raises more.
constexpr int closing_sequence_count(int max_raises) {
    return 2 * max_raises + 1;
}

// Opened with a check, the id is the raise count. Opened with a bet,
// the id is max_raises above that. With three raises allowed the ids
// are 0 check-check, 1 check-bet-call, 2 check-bet-raise-call,
// 3 check-bet-raise-raise-call, 4 bet-call, 5 bet-raise-call and
// 6 bet-raise-raise-call.
inline int closing_sequence(int raises, bool opened_with_check,
                            int max_raises) {
    return opened_with_check ? raises : raises + max_raises;
}

inline Action last_action(const State& state) {
    assert(state.round_len != 0);
    return state.history[static_cast<std::size_t>(state.history_len) - 1];
}

// Kuhn's own 0 is check; Kuhn never calls this.
inline bool folded(const State& state) {
    return state.round_len != 0 && last_action(state) == kActionFold;
}

// The player to act faces a bet: the round's last action is a raise.
inline bool facing_bet(const State& state) {
    return state.round_len != 0 && last_action(state) == kActionRaise;
}

// A lone opening check leaves the round open.
inline bool round_closed(const State& state) {
    if (state.round_len < 2) return false;
    if (last_action(state) != kActionCallCheck) return false;
    Action previous =
        state.history[static_cast<std::size_t>(state.history_len) - 2];
    return previous == kActionCallCheck || previous == kActionRaise;
}

inline bool opened_with_check(const State& state) {
    if (state.round_len == 0) return false;
    Action first = state.history[static_cast<std::size_t>(state.history_len) -
                                 state.round_len];
    return first == kActionCallCheck;
}

// 2 * raises + 1 if the round opened with a check: 0 to 7 with three
// raises allowed. The seat to act is round_len % 2: codes 0, 3, 4, 7
// leave seat 0 to act and 1, 2, 5, 6 seat 1.
inline int round_progress(const State& state) {
    return static_cast<int>(state.raises_used) * 2 +
           (opened_with_check(state) ? 1 : 0);
}

// Fold needs a bet to face. Raise is dropped once max_raises is reached.
template <std::size_t N>
inline FixedVector<Action, N> round_legal_actions(const State& state,
                                                  int max_raises) {
    if (!facing_bet(state)) return {kActionCallCheck, kActionRaise};
    if (state.raises_used < max_raises)
        return {kActionFold, kActionCallCheck, kActionRaise};
    return {kActionFold, kActionCallCheck};
}

// The winner gains the loser's contribution. nullopt is a tie.
inline double settle(const std::array<int, 2>& contributions,
                     std::optional<Player> winner, Player player) {
    if (!winner.has_value()) return 0.0;
    if (player == *winner)
        return static_cast<double>(contributions[opponent(*winner)]);
    return -static_cast<double>(contributions[player]);
}

inline void append_action(State& next, Action action) {
    assert(next.history_len < kMaxHistory);
    next.history[next.history_len++] = static_cast<std::uint8_t>(action);
}

// First free slot: player 0's hole card, then player 1's, then the board.
inline void deal_card(State& next, int card) {
    if (next.hole_cards[0] == -1) {
        next.hole_cards[0] = static_cast<std::int8_t>(card);
    } else if (next.hole_cards[1] == -1) {
        next.hole_cards[1] = static_cast<std::int8_t>(card);
    } else {
        next.board_cards[next.board_count++] = static_cast<std::int8_t>(card);
    }
}

inline void begin_next_betting_round(State& state) {
    state.round_len = 0;
    state.raises_used = 0;
    state.level_to_match = 0;
    state.round_contribution = {0, 0};
}

// A board card stores the closing-sequence code before the round
// counters reset. max_raises is the cap of the round being closed;
// a hole-card deal ignores it. Call only after append_action has
// recorded the chance entry as history's last element.
inline void deal_and_advance(State& state, int card, int max_raises) {
    bool board_card = state.hole_cards[0] != -1 && state.hole_cards[1] != -1;
    if (board_card) {
        assert(state.board_count < state.closing.size());
        assert(state.history_len > state.round_len);
        assert(state.history[static_cast<std::size_t>(state.history_len) - 1] ==
               static_cast<std::uint8_t>(kChanceCardOffset + card));
        std::size_t first_index =
            static_cast<std::size_t>(state.history_len) - state.round_len - 1;
        Action first_action = state.history[first_index];
        bool opened = first_action == kActionCallCheck;
        state.closing[state.board_count] =
            static_cast<std::uint8_t>(closing_sequence(
                static_cast<int>(state.raises_used), opened, max_raises));
    }
    deal_card(state, card);
    if (board_card) begin_next_betting_round(state);
}

// bet_size is the fixed bet of this round. The action is already the
// last history entry. A fold adds nothing. A raise lifts the level
// by bet_size; a call or raise then matches it.
inline void apply_betting_action(State& state, Action action, int bet_size) {
    assert(action == kActionFold || action == kActionCallCheck ||
           action == kActionRaise);
    std::size_t acting =
        static_cast<std::size_t>(state.round_len) % kPlayerCount;
    assert(state.round_len < kMaxActionsPerRound);

    if (action == kActionRaise) {
        state.level_to_match += bet_size;
        ++state.raises_used;
    }
    if (action == kActionRaise || action == kActionCallCheck) {
        state.contribution[acting] +=
            state.level_to_match - state.round_contribution[acting];
        state.round_contribution[acting] = state.level_to_match;
    }
    ++state.round_len;
}

// Probabilities are 1/remaining, in ascending card order — the same
// values ExternalSamplingSolver::sample_index sums.
inline FixedVector<ChanceOutcome, kMaxChanceOutcomes> deal_outcomes(
    const State& state, int deck_size) {
    assert(deck_size > 0);
    assert(deck_size <= static_cast<int>(kMaxChanceOutcomes));
    std::array<std::uint8_t, kMaxChanceOutcomes> dealt{};
    int excluded_count = 0;
    if (state.hole_cards[0] != -1) {
        dealt[static_cast<std::size_t>(state.hole_cards[0])] = 1;
        ++excluded_count;
    }
    if (state.hole_cards[1] != -1) {
        dealt[static_cast<std::size_t>(state.hole_cards[1])] = 1;
        ++excluded_count;
    }
    for (std::uint8_t index = 0; index < state.board_count; ++index) {
        dealt[static_cast<std::size_t>(state.board_cards[index])] = 1;
        ++excluded_count;
    }

    int remaining_count = deck_size - excluded_count;
    FixedVector<ChanceOutcome, kMaxChanceOutcomes> outcomes;
    for (int card = 0; card < deck_size; ++card) {
        if (dealt[static_cast<std::size_t>(card)] != 0) continue;
        outcomes.push_back({kChanceCardOffset + card, 1.0 / remaining_count});
    }
    return outcomes;
}

// Running sums of 1/n, one prefix per n in 1..52, added one term at a
// time in the order sample_index adds them: the checkpoint depends on
// these exact bits, which k * (1/n) would not reproduce.
struct UniformPrefix {
    static constexpr int kMaxN = static_cast<int>(kMaxChanceOutcomes);
    static constexpr int kSumCount = kMaxN * (kMaxN - 1) / 2;
    std::array<double, kSumCount> sums{};
    std::array<int, kMaxN + 1> offset{};

    UniformPrefix() {
        int cursor = 0;
        for (int n = 1; n <= kMaxN; ++n) {
            offset[static_cast<std::size_t>(n)] = cursor;
            double probability = 1.0 / n;
            double cumulative = 0.0;
            for (int i = 0; i + 1 < n; ++i) {
                cumulative += probability;
                sums[static_cast<std::size_t>(cursor++)] = cumulative;
            }
        }
    }
};

inline const UniformPrefix& uniform_prefix() {
    static const UniformPrefix prefix;
    return prefix;
}

// Card at the index sample_index would return for this roll on
// deal_outcomes(state, deck_size). roll is one draw from
// uniform_real_distribution<double>(0, 1).
inline Action sample_undealt_card(const State& state, int deck_size,
                                  double roll) {
    assert(deck_size > 0);
    assert(deck_size <= UniformPrefix::kMaxN);
    static_assert(UniformPrefix::kMaxN <= 64, "dealt mask is 64 bits");
    std::uint64_t dealt = 0;
    auto mark = [&](int card) {
        if (card >= 0) dealt |= std::uint64_t{1} << card;
    };
    mark(state.hole_cards[0]);
    mark(state.hole_cards[1]);
    for (std::uint8_t index = 0; index < state.board_count; ++index)
        mark(state.board_cards[index]);

    int remaining = deck_size - std::popcount(dealt);
    assert(remaining > 0);
    const UniformPrefix& prefix = uniform_prefix();
    const double* begin =
        prefix.sums.data() + prefix.offset[static_cast<std::size_t>(remaining)];
    const double* end = begin + (remaining - 1);
    int chosen = static_cast<int>(std::upper_bound(begin, end, roll) - begin);

    std::uint64_t undealt = (~dealt) & ((std::uint64_t{1} << deck_size) - 1);
    for (int skip = 0; skip < chosen; ++skip) undealt &= undealt - 1;
    assert(undealt != 0);
    return kChanceCardOffset + std::countr_zero(undealt);
}

// Longest history: per round one opening check, the raises and a
// closing call, plus every chance entry.
constexpr int max_history_depth(int rounds, int max_raises, int chance_events) {
    return rounds * (max_raises + 2) + chance_events;
}

}
