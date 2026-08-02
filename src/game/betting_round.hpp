#pragma once

#include <algorithm>
#include <array>
#include <cassert>
#include <cstddef>
#include <cstdint>
#include <string>
#include <utility>
#include <vector>

#include "game/game.hpp"

namespace cfr::game {

constexpr Action kActionFold = 0;
constexpr Action kActionCallCheck = 1;
constexpr Action kActionRaise = 2;

constexpr Action kChanceCardOffset = 100;

constexpr std::size_t kMaxBettingRounds = 3;
constexpr int kHoleCardsDealt = 2;

struct ParsedRounds {
    std::array<std::vector<Action>, kMaxBettingRounds> round;
    int board_cards_dealt = 0;
};

inline ParsedRounds parse_rounds(const State& state) {
    ParsedRounds parsed;
    int chance_events_seen = 0;
    for (std::uint8_t index = 0; index < state.history_len; ++index) {
        Action entry = state.history[index];
        if (entry >= kChanceCardOffset) {
            ++chance_events_seen;
            continue;
        }
        int board_dealt = std::max(0, chance_events_seen - kHoleCardsDealt);
        parsed.round[static_cast<std::size_t>(std::min<int>(board_dealt, kMaxBettingRounds - 1))].push_back(entry);
    }
    parsed.board_cards_dealt = std::max(0, chance_events_seen - kHoleCardsDealt);
    return parsed;
}

inline const std::vector<Action>& active_round_actions(const ParsedRounds& parsed) {
    return parsed.round[static_cast<std::size_t>(std::min<int>(parsed.board_cards_dealt, kMaxBettingRounds - 1))];
}

const std::vector<Action>& active_round_actions(ParsedRounds&&) = delete;

inline bool folded(const std::vector<Action>& round_actions) {
    return !round_actions.empty() && round_actions.back() == kActionFold;
}

inline bool round_closed(const std::vector<Action>& round_actions) {
    if (round_actions.size() < 2) return false;
    if (round_actions.back() != kActionCallCheck) return false;
    Action previous = round_actions[round_actions.size() - 2];
    return previous == kActionCallCheck || previous == kActionRaise;
}

inline Player round_actor(const std::vector<Action>& round_actions) {
    return static_cast<Player>(round_actions.size() % 2);
}

inline int raises_used(const std::vector<Action>& round_actions) {
    return static_cast<int>(std::count(round_actions.begin(), round_actions.end(), kActionRaise));
}

inline int round_progress(const std::vector<Action>& round_actions) {
    bool opened_with_check = !round_actions.empty() && round_actions.front() == kActionCallCheck;
    return raises_used(round_actions) * 2 + (opened_with_check ? 1 : 0);
}

inline std::vector<Action> round_legal_actions(const std::vector<Action>& round_actions, int max_raises) {
    bool facing_wager = !round_actions.empty() && round_actions.back() == kActionRaise;
    if (!facing_wager) {
        return {kActionCallCheck, kActionRaise};
    }
    if (raises_used(round_actions) < max_raises) {
        return {kActionFold, kActionCallCheck, kActionRaise};
    }
    return {kActionFold, kActionCallCheck};
}

inline void apply_round_contributions(const std::vector<Action>& round_actions, int bet_size,
                                       std::array<int, 2>& contribution) {
    std::array<int, 2> street_contribution = {0, 0};
    int level_to_match = 0;
    for (std::size_t index = 0; index < round_actions.size(); ++index) {
        Player actor = static_cast<Player>(index % 2);
        Action action = round_actions[index];
        if (action == kActionRaise) {
            level_to_match += bet_size;
            street_contribution[actor] = level_to_match;
        } else if (action == kActionCallCheck) {
            street_contribution[actor] = level_to_match;
        }
    }
    contribution[0] += street_contribution[0];
    contribution[1] += street_contribution[1];
}

inline double settle(const std::array<int, 2>& contribution, Player winner, Player player) {
    if (winner < 0) return 0.0;
    if (player == winner) return static_cast<double>(contribution[1 - winner]);
    return -static_cast<double>(contribution[player]);
}

inline std::string action_name(Action action) {
    switch (action) {
        case kActionFold: return "fold";
        case kActionCallCheck: return "call";
        case kActionRaise: return "raise";
    }
    return "?";
}

inline void append_action(State& next, Action action) {
    assert(next.history_len < kMaxHistory);
    next.history[next.history_len++] = static_cast<std::uint8_t>(action);
}

inline void deal_card(State& next, int card) {
    if (next.private_cards[0] == -1) {
        next.private_cards[0] = static_cast<std::int8_t>(card);
    } else if (next.private_cards[1] == -1) {
        next.private_cards[1] = static_cast<std::int8_t>(card);
    } else {
        next.public_cards[next.public_count++] = static_cast<std::int8_t>(card);
    }
}

inline std::vector<std::pair<Action, double>> deal_outcomes(const State& state, int deck_size) {
    std::vector<int> excluded;
    if (state.private_cards[0] != -1) excluded.push_back(state.private_cards[0]);
    if (state.private_cards[1] != -1) excluded.push_back(state.private_cards[1]);
    for (std::uint8_t index = 0; index < state.public_count; ++index) excluded.push_back(state.public_cards[index]);

    int remaining_count = deck_size - static_cast<int>(excluded.size());
    std::vector<std::pair<Action, double>> outcomes;
    for (int card = 0; card < deck_size; ++card) {
        if (std::find(excluded.begin(), excluded.end(), card) == excluded.end()) {
            outcomes.emplace_back(kChanceCardOffset + card, 1.0 / remaining_count);
        }
    }
    return outcomes;
}

constexpr int max_history_depth(int rounds, int max_raises, int chance_events) {
    return rounds * (max_raises + 2) + chance_events;
}

}
