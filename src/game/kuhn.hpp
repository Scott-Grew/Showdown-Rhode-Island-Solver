#pragma once

#include <cstddef>

#include <cstdint>
#include <utility>
#include <vector>

#include "game/game.hpp"

namespace cfr::game {

// Kuhn card ids, ordered by strength.
constexpr int kKuhnJack = 0;
constexpr int kKuhnQueen = 1;
constexpr int kKuhnKing = 2;
constexpr int kKuhnDeckSize = 3;

// Kuhn betting action codes. Dealt cards enter the history as raw
// card ids, without kChanceCardOffset.
constexpr Action kKuhnActionCheck = 0;
constexpr Action kKuhnActionBet = 1;
constexpr Action kKuhnActionCall = 2;
constexpr Action kKuhnActionFold = 3;

// Kuhn poker: three cards, one private card each, a one-chip ante
// and a single one-chip bet.
class KuhnGame {
public:
    static constexpr std::size_t kMaxActions = 2;

    // The empty state, before any card is dealt.
    State initial_state() const;
    // True once the hand is over, by a fold or a showdown.
    bool is_terminal(const State& state) const;
    // True while a card is due to be dealt.
    bool is_chance(const State& state) const;
    // Player to act, or -1 at a chance node.
    Player current_player(const State& state) const;
    // Legal actions at a decision node, in strategy slot order.
    std::vector<Action> legal_actions(const State& state) const;
    // Returns the state after a betting action or a dealt card.
    State apply_action(const State& state, Action action) const;
    // Chips won by player at a terminal state; the game is
    // zero-sum.
    double terminal_utility(const State& state, Player player) const;
    // Text key of what the acting player knows: cards seen and
    // betting.
    InfoSetKey infoset_label(const State& state) const;
    // Number of infosets, the row count of a solver table.
    std::uint32_t infoset_count() const;
    // Dense row of the acting player's infoset, below
    // infoset_count.
    std::uint32_t infoset_index(const State& state) const;
    // Each possible deal at a chance node with its probability.
    std::vector<std::pair<Action, double>> chance_outcomes(const State& state) const;

private:

    // The history without its two leading card entries.
    static std::vector<Action> betting_history(const State& state);

    // Chips won by player 0 at a terminal state.
    static double player0_utility(const State& state);

    // Decision point reached by the betting: 0 opening, 1 after a
    // check, 2 after a bet, 3 after check then bet.
    static int betting_stage(const std::vector<Action>& betting);
};

}
