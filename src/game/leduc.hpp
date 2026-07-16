#pragma once

#include <utility>
#include <vector>

#include "game/game.hpp"

namespace cfr::game {

// Leduc hold'em (Southey et al. 2005 standard variant): the standard "medium"
// benchmark game between Kuhn and full poker — two betting rounds plus a
// public card, large enough that hand strength genuinely interacts with
// betting. Leduc's own 6-card deck (2 suits x {J, Q, K}) is distinct from
// cfr::Card's 52-card encoding: card ids are 0..5 with rank = card / 2
// (0=J, 1=Q, 2=K) and suit = card % 2. Suit never affects hand strength (no
// flushes in Leduc); it exists only so the deck has 6 distinct cards to deal
// without replacement.
constexpr int kLeducDeckSize = 6;

// Betting actions, identical across both rounds — only the chip amount they
// move differs (round 1 bets/raises are worth kRound1Bet, round 2 kRound2Bet).
constexpr Action kActionFold = 0;
constexpr Action kActionCallCheck = 1;  // check when no wager is pending, call when facing one
constexpr Action kActionRaise = 2;      // bet when opening, raise when facing a wager

constexpr int kRound1Bet = 2;
constexpr int kRound2Bet = 4;
constexpr int kMaxRaisesPerRound = 2;

// Chance actions (card deals) are offset well clear of the three betting
// action ids above so a flat history vector can tell "this entry was a deal"
// from "this entry was a bet" by value alone, with no positional bookkeeping
// needed to tell the two rounds' actions apart.
constexpr Action kChanceCardOffset = 100;

class LeducGame : public Game {
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
};

}  // namespace cfr::game
