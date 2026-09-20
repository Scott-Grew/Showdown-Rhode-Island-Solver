// Fixed-limit betting shared by Leduc and Rhode Island: splitting a
// history into rounds, legal actions, chip contributions and payoffs.
// Kuhn codes its own shorter betting and does not use this file.

#pragma once

#include <algorithm>
#include <array>
#include <cassert>
#include <cstddef>
#include <cstdint>
#include <span>
#include <string>
#include <utility>
#include <vector>

#include "game/game.hpp"

namespace cfr::game {

// Betting action codes shared by Leduc and Rhode Island.
constexpr Action kActionFold = 0;
constexpr Action kActionCallCheck = 1;
constexpr Action kActionRaise = 2;

// State::history for Leduc and Rhode Island, in play order:
//
//   [hole 0][hole 1][round 1 actions][board][round 2 actions]...
//
// A history entry of kChanceCardOffset + card is a dealt card; any
// entry below the offset is a betting action.
constexpr Action kChanceCardOffset = 100;

// Array bounds for ParsedRounds, and the hole-card deal count.
constexpr std::size_t kMaxBettingRounds = 3;
constexpr int kHoleCardsDealt = 2;
constexpr std::size_t kMaxActionsPerRound = 8;

// The betting actions of a history split by round, plus the number
// of board cards dealt so far.
struct ParsedRounds {
    std::array<std::array<Action, kMaxActionsPerRound>, kMaxBettingRounds>
        action{};
    std::array<std::uint8_t, kMaxBettingRounds> count{};
    int board_cards_dealt = 0;

    // View of one round's actions; valid while this object lives.
    std::span<const Action> round(std::size_t index) const {
        return std::span<const Action>(action[index].data(), count[index]);
    }
};

// Splits a history into rounds. An action belongs to the round
// whose index is the number of board cards dealt before it.
inline ParsedRounds parse_rounds(const State& state) {
    ParsedRounds parsed;
    int chance_events_seen = 0;
    for (std::uint8_t index = 0; index < state.history_len; ++index) {
        Action entry = state.history[index];
        if (entry >= kChanceCardOffset) {
            ++chance_events_seen;
            continue;
        }
        // The first two chance entries are hole cards, not the board.
        int board_dealt = std::max(0, chance_events_seen - kHoleCardsDealt);
        std::size_t round_index = static_cast<std::size_t>(
            std::min<int>(board_dealt, kMaxBettingRounds - 1));
        assert(parsed.count[round_index] < kMaxActionsPerRound);
        parsed.action[round_index][parsed.count[round_index]++] = entry;
    }
    parsed.board_cards_dealt =
        std::max(0, chance_events_seen - kHoleCardsDealt);
    return parsed;
}

// Actions of the round in progress. The span points into parsed.
inline std::span<const Action> active_round_actions(
    const ParsedRounds& parsed) {
    return parsed.round(static_cast<std::size_t>(
        std::min<int>(parsed.board_cards_dealt, kMaxBettingRounds - 1)));
}

// Deleted so the span can never point into a temporary.
std::span<const Action> active_round_actions(ParsedRounds&&) = delete;

// True when the last action of the round is a fold.
inline bool folded(std::span<const Action> round_actions) {
    return !round_actions.empty() && round_actions.back() == kActionFold;
}

// True when the round ended in a call, or in a check answering a
// check. A lone opening check leaves the round open.
inline bool round_closed(std::span<const Action> round_actions) {
    if (round_actions.size() < 2) return false;
    if (round_actions.back() != kActionCallCheck) return false;
    Action previous = round_actions[round_actions.size() - 2];
    return previous == kActionCallCheck || previous == kActionRaise;
}

// Player to act in this round. Player 0 opens every round.
inline Player round_acting_player(std::span<const Action> round_actions) {
    return static_cast<Player>(round_actions.size() % 2);
}

// Number of bets and raises in the round; an opening bet counts.
inline int raises_used(std::span<const Action> round_actions) {
    return static_cast<int>(
        std::count(round_actions.begin(), round_actions.end(), kActionRaise));
}

// Dense code for the round so far: twice the raise count, plus one
// when the round opened with a check.
inline int round_progress(std::span<const Action> round_actions) {
    bool opened_with_check =
        !round_actions.empty() && round_actions.front() == kActionCallCheck;
    return raises_used(round_actions) * 2 + (opened_with_check ? 1 : 0);
}

// Number of distinct round_progress values under a raise cap.
constexpr int round_progress_count(int max_raises) {
    return 2 * max_raises + 2;
}

// Legal actions in ascending code order. Fold needs a bet to face,
// and raise is dropped once max_raises is reached.
inline std::vector<Action> round_legal_actions(
    std::span<const Action> round_actions, int max_raises) {
    bool facing_bet =
        !round_actions.empty() && round_actions.back() == kActionRaise;
    if (!facing_bet) {
        return {kActionCallCheck, kActionRaise};
    }
    if (raises_used(round_actions) < max_raises) {
        return {kActionFold, kActionCallCheck, kActionRaise};
    }
    return {kActionFold, kActionCallCheck};
}

// Adds each player's chips for one round into contributions. Every
// raise lifts the level to match by bet_size.
inline void apply_round_contributions(std::span<const Action> round_actions,
                                      int bet_size,
                                      std::array<int, 2>& contributions) {
    std::array<int, 2> round_contributions = {0, 0};
    int level_to_match = 0;
    for (std::size_t index = 0; index < round_actions.size(); ++index) {
        Player acting_player = static_cast<Player>(index % 2);
        Action action = round_actions[index];
        if (action == kActionRaise) {
            level_to_match += bet_size;
            round_contributions[acting_player] = level_to_match;
        } else if (action == kActionCallCheck) {
            round_contributions[acting_player] = level_to_match;
        }
    }
    contributions[0] += round_contributions[0];
    contributions[1] += round_contributions[1];
}

// Showdown winner when the hands tie.
constexpr Player kNoWinner = -1;

// Payoff in chips to player. The winner gains the loser's
// contribution, and kNoWinner is a tie that pays zero.
inline double settle(const std::array<int, 2>& contributions, Player winner,
                     Player player) {
    if (winner == kNoWinner) return 0.0;
    if (player == winner) return static_cast<double>(contributions[1 - winner]);
    return -static_cast<double>(contributions[player]);
}

// Text for a betting action, used in infoset labels.
inline std::string action_name(Action action) {
    switch (action) {
        case kActionFold: return "fold";
        case kActionCallCheck: return "call";
        case kActionRaise: return "raise";
    }
    return "?";
}

// Appends one entry to the history. Asserts on overflow.
inline void append_action(State& next, Action action) {
    assert(next.history_len < kMaxHistory);
    next.history[next.history_len++] = static_cast<std::uint8_t>(action);
}

// Stores a dealt card in the first free slot: player 0's hole card,
// then player 1's, then the board.
inline void deal_card(State& next, int card) {
    if (next.hole_cards[0] == -1) {
        next.hole_cards[0] = static_cast<std::int8_t>(card);
    } else if (next.hole_cards[1] == -1) {
        next.hole_cards[1] = static_cast<std::int8_t>(card);
    } else {
        next.board_cards[next.board_count++] = static_cast<std::int8_t>(card);
    }
}

// Uniform chance outcomes over the undealt cards, each encoded as
// kChanceCardOffset + card.
inline std::vector<ChanceOutcome> deal_outcomes(const State& state,
                                                int deck_size) {
    std::vector<int> excluded;
    if (state.hole_cards[0] != -1) excluded.push_back(state.hole_cards[0]);
    if (state.hole_cards[1] != -1) excluded.push_back(state.hole_cards[1]);
    for (std::uint8_t index = 0; index < state.board_count; ++index)
        excluded.push_back(state.board_cards[index]);

    int remaining_count = deck_size - static_cast<int>(excluded.size());
    std::vector<ChanceOutcome> outcomes;
    for (int card = 0; card < deck_size; ++card) {
        if (std::find(excluded.begin(), excluded.end(), card) ==
            excluded.end()) {
            outcomes.push_back(
                {kChanceCardOffset + card, 1.0 / remaining_count});
        }
    }
    return outcomes;
}

// Longest possible history: per round one opening check, the
// raises and a closing call, plus every chance entry.
constexpr int max_history_depth(int rounds, int max_raises, int chance_events) {
    return rounds * (max_raises + 2) + chance_events;
}

}
