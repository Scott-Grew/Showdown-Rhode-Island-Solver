#include "game/leduc.hpp"

#include <algorithm>
#include <array>
#include <cstddef>
#include <string>

namespace cfr::game {

namespace {

// A single betting round's actions, split out of the flat history by the
// chance-event count: 2 private deals precede round 1's actions, and a 3rd
// chance event (the public card) precedes round 2's.
struct ParsedHistory {
    std::vector<Action> round1_actions;
    std::vector<Action> round2_actions;
    bool public_card_dealt = false;
};

ParsedHistory parse_history(const State& state) {
    ParsedHistory parsed;
    int chance_events_seen = 0;
    for (Action entry : state.history) {
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
    return !round_actions.empty() && round_actions.back() == kActionFold;
}

// A round closes when the most recent action matches a pending wager (a call)
// or is the second consecutive check. Folding ends the whole hand rather than
// just the round, so it's handled separately by callers, not here.
bool round_closed(const std::vector<Action>& round_actions) {
    if (round_actions.size() < 2) return false;
    if (round_actions.back() != kActionCallCheck) return false;
    Action previous = round_actions[round_actions.size() - 2];
    return previous == kActionCallCheck || previous == kActionRaise;
}

// Heads-up betting alternates strictly within a round; player 0 always opens.
Player round_actor(const std::vector<Action>& round_actions) {
    return static_cast<Player>(round_actions.size() % 2);
}

std::vector<Action> round_legal_actions(const std::vector<Action>& round_actions) {
    if (round_actions.empty()) {
        return {kActionCallCheck, kActionRaise};  // opening action: nothing to fold to yet
    }
    bool facing_wager = round_actions.back() == kActionRaise;
    if (!facing_wager) {
        return {kActionCallCheck, kActionRaise};  // only reachable after a bare opening check
    }
    int raises_used = static_cast<int>(std::count(round_actions.begin(), round_actions.end(), kActionRaise));
    if (raises_used < kMaxRaisesPerRound) {
        return {kActionFold, kActionCallCheck, kActionRaise};
    }
    return {kActionFold, kActionCallCheck};  // raise cap hit — must fold or call
}

int bet_size_for_round(bool public_card_dealt) {
    return public_card_dealt ? kRound2Bet : kRound1Bet;
}

void apply_round_contributions(const std::vector<Action>& round_actions, int bet_size,
                                std::array<int, 2>& contribution) {
    for (std::size_t i = 0; i < round_actions.size(); ++i) {
        Player actor = static_cast<Player>(i % 2);
        Action action = round_actions[i];
        if (action == kActionRaise) {
            contribution[actor] += bet_size;
        } else if (action == kActionCallCheck && i > 0 && round_actions[i - 1] == kActionRaise) {
            contribution[actor] += bet_size;
        }
    }
}

std::array<int, 2> contributions(const State& state) {
    std::array<int, 2> contribution = {1, 1};  // ante
    ParsedHistory parsed = parse_history(state);
    apply_round_contributions(parsed.round1_actions, kRound1Bet, contribution);
    if (parsed.public_card_dealt) {
        apply_round_contributions(parsed.round2_actions, kRound2Bet, contribution);
    }
    return contribution;
}

// +1 => player 0's hand wins, -1 => player 1's, 0 => tie (split pot). Pairing
// with the public card beats any non-pair; otherwise higher rank wins.
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
        case kActionFold: return "fold";
        case kActionCallCheck: return "call";
        case kActionRaise: return "raise";
    }
    return "?";
}

}  // namespace

State LeducGame::initial_state() const {
    State state;
    state.private_cards = {-1, -1};
    state.pot = 0;
    return state;
}

bool LeducGame::is_chance(const State& state) const {
    if (state.private_cards[0] == -1 || state.private_cards[1] == -1) return true;
    ParsedHistory parsed = parse_history(state);
    if (parsed.public_card_dealt) return false;
    return round_closed(parsed.round1_actions) && !folded(parsed.round1_actions);
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
        next.history.push_back(action);
        int card = action - kChanceCardOffset;
        if (state.private_cards[0] == -1) {
            next.private_cards[0] = card;
            next.pot += 1;  // ante
        } else if (state.private_cards[1] == -1) {
            next.private_cards[1] = card;
            next.pot += 1;  // ante
        } else {
            next.public_cards.push_back(card);
        }
        return next;
    }

    next.history.push_back(action);
    bool public_card_dealt = !state.public_cards.empty();
    if (action == kActionRaise) {
        next.pot += bet_size_for_round(public_card_dealt);
    } else if (action == kActionCallCheck) {
        ParsedHistory parsed = parse_history(state);
        const std::vector<Action>& active_round = public_card_dealt ? parsed.round2_actions : parsed.round1_actions;
        if (!active_round.empty() && active_round.back() == kActionRaise) {
            next.pot += bet_size_for_round(public_card_dealt);
        }
    }
    return next;
}

double LeducGame::terminal_utility(const State& state, Player player) const {
    std::array<int, 2> contribution = contributions(state);
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
    if (showdown_result == 0) return 0.0;  // tie: split pot, utility 0 each
    Player winner = showdown_result > 0 ? 0 : 1;
    return player == winner ? static_cast<double>(pot - contribution[winner])
                            : -static_cast<double>(contribution[player]);
}

InfoSetKey LeducGame::infoset_key(const State& state) const {
    Player player = current_player(state);
    ParsedHistory parsed = parse_history(state);

    std::string key = "P" + std::to_string(player) + ":" + card_name(state.private_cards[player]);
    if (!state.public_cards.empty()) {
        key += "|" + card_name(state.public_cards[0]);
    }
    key += ":";
    for (Action action : parsed.round1_actions) key += action_name(action) + ",";
    key += ";";
    for (Action action : parsed.round2_actions) key += action_name(action) + ",";
    return key;
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

}  // namespace cfr::game
