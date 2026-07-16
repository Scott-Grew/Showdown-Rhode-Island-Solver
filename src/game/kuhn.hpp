#pragma once

#include <utility>
#include <vector>

#include "game/game.hpp"

namespace cfr::game {

// Kuhn poker (Kuhn 1950): the canonical 3-card, single-round
// imperfect-information poker game — ground truth for CFR convergence checks
// in M1. The deck here is Kuhn's own tiny 3-card deck {J, Q, K}, distinct
// from cfr::Card's 52-card encoding; card ids are simply 0 = J, 1 = Q, 2 = K.
constexpr int kKuhnJack = 0;
constexpr int kKuhnQueen = 1;
constexpr int kKuhnKing = 2;
constexpr int kKuhnDeckSize = 3;

// Player actions taken after both cards are dealt. These share Action's int
// type with the chance-node card ids above, but the two spaces never collide
// at runtime: which one an Action belongs to is determined by the state's
// dealing phase (is_chance), not by the raw value.
constexpr Action kActionCheck = 0;
constexpr Action kActionBet = 1;
constexpr Action kActionCall = 2;
constexpr Action kActionFold = 3;

class KuhnGame : public Game {
public:
    State initial_state() const override;
    bool is_terminal(const State& state) const override;
    bool is_chance(const State& state) const override;
    Player current_player(const State& state) const override;
    std::vector<Action> legal_actions(const State& state) const override;
    State apply_action(const State& state, Action action) const override;
    double terminal_utility(const State& state, Player player) const override;
    InfoSetKey infoset_key(const State& state) const override;
    std::vector<std::pair<Action, double>> chance_outcomes(const State& state) const override;

private:
    // Betting actions only, with the two chance deals stripped off the front
    // of history. Only valid once both players are dealt (is_chance false) —
    // every caller here only reaches it in that phase.
    static std::vector<Action> betting_history(const State& state);

    // Player 0's utility at a showdown or fold terminal; player 1's is always
    // its negation (Kuhn is two-player zero-sum by construction).
    static double player0_utility(const State& state);
};

}  // namespace cfr::game
