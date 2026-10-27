// Test oracle: Leduc hold'em, small enough to walk the whole tree.

#pragma once

#include <cstddef>
#include <cstdint>

#include "game/betting_round.hpp"
#include "game/game.hpp"

namespace cfr::game {

// Six cards: ranks J, Q, K in two suits. Rank is card / 2.
constexpr int kLeducDeckSize = 6;

// Stakes in chips: fixed bet per round, raise cap and ante.
constexpr int kLeducRound1Bet = 2;
constexpr int kLeducRound2Bet = 4;
constexpr int kLeducMaxRaisesPerRound = 2;
constexpr int kLeducAnte = 1;

// Leduc hold'em: one hole card each, one board card dealt after
// round 1, and two betting rounds.
class LeducGame {
public:
    static constexpr std::size_t kMaxActions = 3;

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
};

static_assert(GameLike<LeducGame>);
static_assert(LabelledGame<LeducGame>);

}
