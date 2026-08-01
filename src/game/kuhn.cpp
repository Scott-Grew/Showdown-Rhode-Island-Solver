#include "game/kuhn.hpp"

#include <string>

namespace cfr::game {

namespace {

std::string card_name(int card) {
    switch (card) {
        case kKuhnJack: return "J";
        case kKuhnQueen: return "Q";
        case kKuhnKing: return "K";
    }
    return "?";
}

std::string action_name(Action action) {
    switch (action) {
        case kKuhnActionCheck: return "check";
        case kKuhnActionBet: return "bet";
        case kKuhnActionCall: return "call";
        case kKuhnActionFold: return "fold";
    }
    return "?";
}

}

std::vector<Action> KuhnGame::betting_history(const State& state) {
    std::vector<Action> betting;
    for (std::uint8_t index = 2; index < state.history_len; ++index) {
        betting.push_back(state.history[index]);
    }
    return betting;
}

State KuhnGame::initial_state() const {
    return State{};
}

bool KuhnGame::is_chance(const State& state) const {
    return state.private_cards[0] == -1 || state.private_cards[1] == -1;
}

bool KuhnGame::is_terminal(const State& state) const {
    if (is_chance(state)) return false;
    std::vector<Action> betting = betting_history(state);
    if (betting.size() < 2) return false;
    if (betting.size() == 2) return !(betting[0] == kKuhnActionCheck && betting[1] == kKuhnActionBet);
    return true;
}

Player KuhnGame::current_player(const State& state) const {
    if (is_chance(state)) return -1;
    std::vector<Action> betting = betting_history(state);
    if (betting.empty()) return 0;
    if (betting.size() == 1) return 1;
    return 0;
}

std::vector<Action> KuhnGame::legal_actions(const State& state) const {
    std::vector<Action> betting = betting_history(state);
    if (betting.empty()) return {kKuhnActionCheck, kKuhnActionBet};
    if (betting.size() == 1) {
        if (betting[0] == kKuhnActionCheck) return {kKuhnActionCheck, kKuhnActionBet};
        return {kKuhnActionCall, kKuhnActionFold};
    }
    return {kKuhnActionCall, kKuhnActionFold};
}

State KuhnGame::apply_action(const State& state, Action action) const {
    State next = state;
    if (is_chance(state)) {
        if (next.private_cards[0] == -1) {
            next.private_cards[0] = action;
        } else {
            next.private_cards[1] = action;
        }
        next.history[next.history_len++] = static_cast<std::uint8_t>(action);
        next.pot += 1;
        return next;
    }

    next.history[next.history_len++] = static_cast<std::uint8_t>(action);
    if (action == kKuhnActionBet || action == kKuhnActionCall) {
        next.pot += 1;
    }
    return next;
}

double KuhnGame::player0_utility(const State& state) {
    std::vector<Action> betting = betting_history(state);
    bool player0_has_higher_card = state.private_cards[0] > state.private_cards[1];

    if (betting.size() == 2) {
        if (betting[0] == kKuhnActionCheck && betting[1] == kKuhnActionCheck) {
            return player0_has_higher_card ? 1.0 : -1.0;
        }
        if (betting[0] == kKuhnActionBet && betting[1] == kKuhnActionCall) {
            return player0_has_higher_card ? 2.0 : -2.0;
        }
        return 1.0;
    }

    if (betting[2] == kKuhnActionCall) {
        return player0_has_higher_card ? 2.0 : -2.0;
    }
    return -1.0;
}

double KuhnGame::terminal_utility(const State& state, Player player) const {
    double utility = player0_utility(state);
    return player == 0 ? utility : -utility;
}

InfoSetKey KuhnGame::infoset_label(const State& state) const {
    Player player = current_player(state);
    std::vector<Action> betting = betting_history(state);

    std::string key = "P" + std::to_string(player) + ":" + card_name(state.private_cards[player]) + ":";
    for (Action action : betting) {
        key += action_name(action) + ",";
    }
    return key;
}

int KuhnGame::betting_stage(const std::vector<Action>& betting) {
    if (betting.empty()) return 0;
    if (betting.size() == 1) return betting[0] == kKuhnActionCheck ? 1 : 2;
    return 3;
}

std::uint32_t KuhnGame::infoset_count() const {
    return 4 * kKuhnDeckSize;
}

std::uint32_t KuhnGame::infoset_index(const State& state) const {
    Player player = current_player(state);
    std::vector<Action> betting = betting_history(state);
    int stage = betting_stage(betting);
    return static_cast<std::uint32_t>(stage * kKuhnDeckSize + state.private_cards[player]);
}

std::vector<std::pair<Action, double>> KuhnGame::chance_outcomes(const State& state) const {
    std::vector<std::pair<Action, double>> outcomes;
    if (state.private_cards[0] == -1) {
        for (int card = 0; card < kKuhnDeckSize; ++card) {
            outcomes.emplace_back(card, 1.0 / kKuhnDeckSize);
        }
        return outcomes;
    }

    for (int card = 0; card < kKuhnDeckSize; ++card) {
        if (card != state.private_cards[0]) {
            outcomes.emplace_back(card, 1.0 / (kKuhnDeckSize - 1));
        }
    }
    return outcomes;
}

}
