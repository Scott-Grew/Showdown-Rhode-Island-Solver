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
        case kActionCheck: return "check";
        case kActionBet: return "bet";
        case kActionCall: return "call";
        case kActionFold: return "fold";
    }
    return "?";
}

}

std::vector<Action> KuhnGame::betting_history(const State& state) {
    return std::vector<Action>(state.history.begin() + 2, state.history.end());
}

State KuhnGame::initial_state() const {
    State state;
    state.private_cards = {-1, -1};
    state.pot = 0;
    return state;
}

bool KuhnGame::is_chance(const State& state) const {
    return state.private_cards[0] == -1 || state.private_cards[1] == -1;
}

bool KuhnGame::is_terminal(const State& state) const {
    if (is_chance(state)) return false;
    std::vector<Action> betting = betting_history(state);
    if (betting.size() < 2) return false;
    if (betting.size() == 2) return !(betting[0] == kActionCheck && betting[1] == kActionBet);
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
    if (betting.empty()) return {kActionCheck, kActionBet};
    if (betting.size() == 1) {
        if (betting[0] == kActionCheck) return {kActionCheck, kActionBet};
        return {kActionCall, kActionFold};
    }
    return {kActionCall, kActionFold};
}

State KuhnGame::apply_action(const State& state, Action action) const {
    State next = state;
    if (is_chance(state)) {
        if (next.private_cards[0] == -1) {
            next.private_cards[0] = action;
        } else {
            next.private_cards[1] = action;
        }
        next.history.push_back(action);
        next.pot += 1;
        return next;
    }

    next.history.push_back(action);
    if (action == kActionBet || action == kActionCall) {
        next.pot += 1;
    }
    return next;
}

double KuhnGame::player0_utility(const State& state) {
    std::vector<Action> betting = betting_history(state);
    bool player0_has_higher_card = state.private_cards[0] > state.private_cards[1];

    if (betting.size() == 2) {
        if (betting[0] == kActionCheck && betting[1] == kActionCheck) {
            return player0_has_higher_card ? 1.0 : -1.0;
        }
        if (betting[0] == kActionBet && betting[1] == kActionCall) {
            return player0_has_higher_card ? 2.0 : -2.0;
        }
        return 1.0;
    }

    if (betting[2] == kActionCall) {
        return player0_has_higher_card ? 2.0 : -2.0;
    }
    return -1.0;
}

double KuhnGame::terminal_utility(const State& state, Player player) const {
    double utility = player0_utility(state);
    return player == 0 ? utility : -utility;
}

InfoSetKey KuhnGame::infoset_key(const State& state) const {
    Player player = current_player(state);
    std::vector<Action> betting = betting_history(state);

    std::string key = "P" + std::to_string(player) + ":" + card_name(state.private_cards[player]) + ":";
    for (Action action : betting) {
        key += action_name(action) + ",";
    }
    return key;
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
