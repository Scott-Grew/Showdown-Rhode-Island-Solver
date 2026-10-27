// Test oracle: Kuhn poker, whose equilibrium value is known analytically.

#pragma once

#include <cstddef>
#include <cstdint>

#include "game/betting_round.hpp"
#include "game/game.hpp"
#include "oracles/labels.hpp"

namespace cfr::game {

// Kuhn card ids, ordered by strength.
constexpr int kKuhnJack = 0;
constexpr int kKuhnQueen = 1;
constexpr int kKuhnKing = 2;
constexpr int kKuhnDeckSize = 3;

// Kuhn betting action codes.
constexpr Action kKuhnActionCheck = 0;
constexpr Action kKuhnActionBet = 1;
constexpr Action kKuhnActionCall = 2;
constexpr Action kKuhnActionFold = 3;

// Kuhn poker: three cards, one hole card each, a one-chip ante
// and a single one-chip bet.
class KuhnGame {
public:
    static constexpr std::size_t kMaxActions = 2;

    State initial_state() const;
    bool is_terminal(const State& state) const;
    bool is_chance(const State& state) const;
    Player current_player(const State& state) const;
    FixedVector<Action, kMaxActions> legal_actions(const State& state) const;
    State apply_action(const State& state, Action action) const;
    double terminal_utility(const State& state, Player player) const;
    InfosetLabel infoset_label(const State& state) const;
    std::uint32_t infoset_count() const;
    std::uint32_t infoset_index(const State& state) const;
    FixedVector<ChanceOutcome, kMaxChanceOutcomes> chance_outcomes(
        const State& state) const;

private:
    // One full Kuhn hand ends in at most three betting actions.
    static constexpr std::size_t kMaxBettingActions = 3;

    static FixedVector<Action, kMaxBettingActions> betting_history(
        const State& state);
    static double player0_utility(const State& state);

    // 0 opening, 1 after a check, 2 after a bet, 3 after check-bet.
    static int betting_progress(
        const FixedVector<Action, kMaxBettingActions>& betting);
};

static_assert(GameLike<KuhnGame>);
static_assert(LabelledGame<KuhnGame>);

}
