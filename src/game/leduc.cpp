#include "game/leduc.hpp"

#include <algorithm>
#include <array>
#include <cstddef>
#include <string>

namespace cfr::game {

namespace {

struct ParsedHistory {
    std::vector<Action> round1_actions;
    std::vector<Action> round2_actions;
    bool public_card_dealt = false;
};

ParsedHistory parse_history(const State& state) {
    ParsedHistory parsed;
    int chance_events_seen = 0;
    for (std::uint8_t index = 0; index < state.history_len; ++index) {
        Action entry = state.history[index];
        if (entry >= kChanceCardOffset) {
            ++chance_events_seen;
            if (chance_events_seen == 3) parsed.public_card_dealt = true;
            continue;
        }
        if (chance_events_seen < 3) {
            parsed.round1_actions.push_back(entry);
        } else {
            parsed.round2_actions.push_back(entry);
        }
    }
    return parsed;
}

bool folded(const std::vector<Action>& round_actions) {
    return !round_actions.empty() && round_actions.back() == kLeducActionFold;
}

bool round_closed(const std::vector<Action>& round_actions) {
    if (round_actions.size() < 2) return false;
    if (round_actions.back() != kLeducActionCallCheck) return false;
    Action previous = round_actions[round_actions.size() - 2];
    return previous == kLeducActionCallCheck || previous == kLeducActionRaise;
}

Player round_actor(const std::vector<Action>& round_actions) {
    return static_cast<Player>(round_actions.size() % 2);
}

std::vector<Action> round_legal_actions(const std::vector<Action>& round_actions) {
    if (round_actions.empty()) {
        return {kLeducActionCallCheck, kLeducActionRaise};
    }
    bool facing_wager = round_actions.back() == kLeducActionRaise;
    if (!facing_wager) {
        return {kLeducActionCallCheck, kLeducActionRaise};
    }
    int raises_used = static_cast<int>(std::count(round_actions.begin(), round_actions.end(), kLeducActionRaise));
    if (raises_used < kMaxRaisesPerRound) {
        return {kLeducActionFold, kLeducActionCallCheck, kLeducActionRaise};
    }
    return {kLeducActionFold, kLeducActionCallCheck};
}

void apply_round_contributions(const std::vector<Action>& round_actions, int bet_size,
                                std::array<int, 2>& contribution) {
    std::array<int, 2> street_contribution = {0, 0};
    int level_to_match = 0;
    for (std::size_t i = 0; i < round_actions.size(); ++i) {
        Player actor = static_cast<Player>(i % 2);
        Action action = round_actions[i];
        if (action == kLeducActionRaise) {
            level_to_match += bet_size;
            street_contribution[actor] = level_to_match;
        } else if (action == kLeducActionCallCheck) {
            street_contribution[actor] = level_to_match;
        }
    }
    contribution[0] += street_contribution[0];
    contribution[1] += street_contribution[1];
}

int compare_hands(const State& state) {
    int public_rank = state.public_cards[0] / 2;
    int rank0 = state.private_cards[0] / 2;
    int rank1 = state.private_cards[1] / 2;
    bool pair0 = rank0 == public_rank;
    bool pair1 = rank1 == public_rank;
    if (pair0 != pair1) return pair0 ? 1 : -1;
    if (rank0 != rank1) return rank0 > rank1 ? 1 : -1;
    return 0;
}

std::string card_name(int card) {
    static const char* const kRankNames[] = {"J", "Q", "K"};
    return kRankNames[card / 2];
}

std::string action_name(Action action) {
    switch (action) {
        case kLeducActionFold: return "fold";
        case kLeducActionCallCheck: return "call";
        case kLeducActionRaise: return "raise";
    }
    return "?";
}

constexpr int kLeducCardRankCount = 3;
constexpr int kLeducRoundStageCount = 6;
constexpr int kLeducRound1EndingCount = 5;

int card_rank(int card) {
    return card / 2;
}

int round_progress(const std::vector<Action>& round_actions) {
    int raises_used = static_cast<int>(std::count(round_actions.begin(), round_actions.end(), kLeducActionRaise));
    bool opened_with_check = !round_actions.empty() && round_actions.front() == kLeducActionCallCheck;
    return raises_used * 2 + (opened_with_check ? 1 : 0);
}

}

std::array<int, 2> leduc_contributions(const State& state) {
    std::array<int, 2> contribution = {kLeducAnte, kLeducAnte};
    ParsedHistory parsed = parse_history(state);
    apply_round_contributions(parsed.round1_actions, kRound1Bet, contribution);
    if (parsed.public_card_dealt) {
        apply_round_contributions(parsed.round2_actions, kRound2Bet, contribution);
    }
    return contribution;
}

State LeducGame::initial_state() const {
    return State{};
}

bool LeducGame::is_chance(const State& state) const {
    if (state.private_cards[0] == -1 || state.private_cards[1] == -1) return true;
    ParsedHistory parsed = parse_history(state);
    if (parsed.public_card_dealt) return false;
    return round_closed(parsed.round1_actions);
}

bool LeducGame::is_terminal(const State& state) const {
    if (is_chance(state)) return false;
    ParsedHistory parsed = parse_history(state);
    if (!parsed.public_card_dealt) return folded(parsed.round1_actions);
    return folded(parsed.round2_actions) || round_closed(parsed.round2_actions);
}

Player LeducGame::current_player(const State& state) const {
    if (is_chance(state)) return -1;
    ParsedHistory parsed = parse_history(state);
    const std::vector<Action>& active_round = parsed.public_card_dealt ? parsed.round2_actions : parsed.round1_actions;
    return round_actor(active_round);
}

std::vector<Action> LeducGame::legal_actions(const State& state) const {
    ParsedHistory parsed = parse_history(state);
    const std::vector<Action>& active_round = parsed.public_card_dealt ? parsed.round2_actions : parsed.round1_actions;
    return round_legal_actions(active_round);
}

State LeducGame::apply_action(const State& state, Action action) const {
    State next = state;
    if (is_chance(state)) {
        next.history[next.history_len++] = static_cast<std::uint8_t>(action);
        int card = action - kChanceCardOffset;
        if (state.private_cards[0] == -1) {
            next.private_cards[0] = card;
        } else if (state.private_cards[1] == -1) {
            next.private_cards[1] = card;
        } else {
            next.public_cards[next.public_count++] = static_cast<std::int8_t>(card);
        }
        return next;
    }

    next.history[next.history_len++] = static_cast<std::uint8_t>(action);
    return next;
}

double LeducGame::terminal_utility(const State& state, Player player) const {
    std::array<int, 2> contribution = leduc_contributions(state);
    int pot = contribution[0] + contribution[1];

    ParsedHistory parsed = parse_history(state);
    const std::vector<Action>& final_round = parsed.public_card_dealt ? parsed.round2_actions : parsed.round1_actions;

    if (folded(final_round)) {
        Player folder = static_cast<Player>((final_round.size() - 1) % 2);
        Player winner = 1 - folder;
        return player == winner ? static_cast<double>(pot - contribution[winner])
                                 : -static_cast<double>(contribution[folder]);
    }

    int showdown_result = compare_hands(state);
    if (showdown_result == 0) return 0.0;
    Player winner = showdown_result > 0 ? 0 : 1;
    return player == winner ? static_cast<double>(pot - contribution[winner])
                            : -static_cast<double>(contribution[player]);
}

InfoSetKey LeducGame::infoset_label(const State& state) const {
    Player player = current_player(state);
    ParsedHistory parsed = parse_history(state);

    std::string key = "P" + std::to_string(player) + ":" + card_name(state.private_cards[player]);
    if (state.public_count != 0) {
        key += "|" + card_name(state.public_cards[0]);
    }
    key += ":";
    for (Action action : parsed.round1_actions) key += action_name(action) + ",";
    key += ";";
    for (Action action : parsed.round2_actions) key += action_name(action) + ",";
    return key;
}

std::uint32_t LeducGame::infoset_count() const {
    return static_cast<std::uint32_t>(kLeducRoundStageCount * kLeducCardRankCount +
                                       kLeducRound1EndingCount * kLeducRoundStageCount * kLeducCardRankCount *
                                           kLeducCardRankCount);
}

std::uint32_t LeducGame::infoset_index(const State& state) const {
    Player player = current_player(state);
    ParsedHistory parsed = parse_history(state);
    int private_rank = card_rank(state.private_cards[player]);

    if (!parsed.public_card_dealt) {
        int round1_stage = round_progress(parsed.round1_actions);
        return static_cast<std::uint32_t>(round1_stage * kLeducCardRankCount + private_rank);
    }

    int round1_ending = round_progress(parsed.round1_actions) - 1;
    int round2_stage = round_progress(parsed.round2_actions);
    int public_rank = card_rank(state.public_cards[0]);

    int round2_offset =
        ((round1_ending * kLeducRoundStageCount + round2_stage) * kLeducCardRankCount + private_rank) *
            kLeducCardRankCount +
        public_rank;
    return static_cast<std::uint32_t>(kLeducRoundStageCount * kLeducCardRankCount + round2_offset);
}

std::vector<std::pair<Action, double>> LeducGame::chance_outcomes(const State& state) const {
    std::vector<int> excluded;
    if (state.private_cards[0] != -1) excluded.push_back(state.private_cards[0]);
    if (state.private_cards[1] != -1) excluded.push_back(state.private_cards[1]);

    int remaining_count = kLeducDeckSize - static_cast<int>(excluded.size());
    std::vector<std::pair<Action, double>> outcomes;
    for (int card = 0; card < kLeducDeckSize; ++card) {
        if (std::find(excluded.begin(), excluded.end(), card) == excluded.end()) {
            outcomes.emplace_back(kChanceCardOffset + card, 1.0 / remaining_count);
        }
    }
    return outcomes;
}

}
