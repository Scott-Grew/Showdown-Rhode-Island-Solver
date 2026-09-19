#include "game/kuhn.hpp"

#include <string>

namespace cfr::game {

namespace {

// Card letter for infoset labels.
std::string card_name(int card) {
    switch (card) {
        case kKuhnJack: return "J";
        case kKuhnQueen: return "Q";
        case kKuhnKing: return "K";
    }
    return "?";
}

// Action word for infoset labels.
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

// Copies the history from index 2 on; the first two entries are
// the dealt cards.
std::vector<Action> KuhnGame::betting_history(const State& state) {
    std::vector<Action> betting;
    for (std::uint8_t index = 2; index < state.history_len; ++index) {
        betting.push_back(state.history[index]);
    }
    return betting;
}

// The empty state, before any card is dealt.
State KuhnGame::initial_state() const {
    return State{};
}

// True while either hole card is still -1.
bool KuhnGame::is_chance(const State& state) const {
    return state.private_cards[0] == -1 || state.private_cards[1] == -1;
}

// Two actions end the hand unless they are check then bet, which
// gives player 0 one more decision.
bool KuhnGame::is_terminal(const State& state) const {
    if (is_chance(state)) return false;
    std::vector<Action> betting = betting_history(state);
    if (betting.size() < 2) return false;
    if (betting.size() == 2) return !(betting[0] == kKuhnActionCheck && betting[1] == kKuhnActionBet);
    return true;
}

// Player 0 opens and also answers a bet made after a check; -1 at
// a chance node.
Player KuhnGame::current_player(const State& state) const {
    if (is_chance(state)) return -1;
    std::vector<Action> betting = betting_history(state);
    if (betting.empty()) return 0;
    if (betting.size() == 1) return 1;
    return 0;
}

// Check or bet until someone bets, then call or fold.
std::vector<Action> KuhnGame::legal_actions(const State& state) const {
    std::vector<Action> betting = betting_history(state);
    if (betting.empty()) return {kKuhnActionCheck, kKuhnActionBet};
    if (betting.size() == 1) {
        if (betting[0] == kKuhnActionCheck) return {kKuhnActionCheck, kKuhnActionBet};
        return {kKuhnActionCall, kKuhnActionFold};
    }
    return {kKuhnActionCall, kKuhnActionFold};
}

// At a chance node the action is the card id dealt to the next
// empty hand; every action is appended to the history.
State KuhnGame::apply_action(const State& state, Action action) const {
    State next = state;
    if (is_chance(state)) {
        if (next.private_cards[0] == -1) {
            next.private_cards[0] = action;
        } else {
            next.private_cards[1] = action;
        }
        next.history[next.history_len++] = static_cast<std::uint8_t>(action);
        return next;
    }

    next.history[next.history_len++] = static_cast<std::uint8_t>(action);
    return next;
}

// A showdown pays 1 after check-check and 2 after a called bet; a
// fold pays 1 to the bettor.
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

// Player 1 receives the negative of player 0's payoff.
double KuhnGame::terminal_utility(const State& state, Player player) const {
    double utility = player0_utility(state);
    return player == 0 ? utility : -utility;
}

// Builds "P<player>:<card>:<actions>" for the acting player.
InfoSetKey KuhnGame::infoset_label(const State& state) const {
    Player player = current_player(state);
    std::vector<Action> betting = betting_history(state);

    std::string key = "P" + std::to_string(player) + ":" + card_name(state.private_cards[player]) + ":";
    for (Action action : betting) {
        key += action_name(action) + ",";
    }
    return key;
}

// 0 opening, 1 after a check, 2 after a bet, 3 after check then
// bet.
int KuhnGame::betting_stage(const std::vector<Action>& betting) {
    if (betting.empty()) return 0;
    if (betting.size() == 1) return betting[0] == kKuhnActionCheck ? 1 : 2;
    return 3;
}

// Four betting stages times three possible private cards.
std::uint32_t KuhnGame::infoset_count() const {
    return 4 * kKuhnDeckSize;
}

// Row is betting stage * deck size + the acting player's card.
std::uint32_t KuhnGame::infoset_index(const State& state) const {
    Player player = current_player(state);
    std::vector<Action> betting = betting_history(state);
    int stage = betting_stage(betting);
    return static_cast<std::uint32_t>(stage * kKuhnDeckSize + state.private_cards[player]);
}

// Player 0's card is uniform over the deck; player 1's is uniform
// over the two cards left.
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
