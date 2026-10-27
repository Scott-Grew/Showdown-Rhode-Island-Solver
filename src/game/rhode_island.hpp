// Rhode Island hold'em, the game this project solves. Its tree is too
// large to walk whole, so it is trained by mccfr_solver.hpp and
// measured by rih_best_response.hpp.

#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <string>

#include "game/betting_round.hpp"
#include "game/card.hpp"
#include "game/game.hpp"

namespace cfr::abstraction {
class RihPartition;
}

namespace cfr::game {

// Stakes in chips: fixed bet per round, raise cap and ante.
constexpr int kRihRound1Bet = 10;
constexpr int kRihRound2Bet = 20;
constexpr int kRihRound3Bet = 20;
constexpr int kRihMaxRaisesPerRound = 3;
constexpr int kRihAnte = 5;

// Chips each player has put in so far, ante included.
std::array<int, 2> rih_contributions(const State& state);

// Two-character card text such as "As" or "Tc".
std::string rih_card_name(int card);

// Rhode Island hold'em: one hole card each, two board cards
// dealt one at a time, and three betting rounds.
class RhodeIslandGame {
public:
    static constexpr std::size_t kMaxActions = 3;

    // Binds the process-lifetime GameShrink partition.
    RhodeIslandGame();
    // The empty state, before any card is dealt.
    State initial_state() const;
    // True once the hand is over, by a fold or a showdown.
    bool is_terminal(const State& state) const;
    // True while a card is due to be dealt.
    bool is_chance(const State& state) const;
    // Player to act at a decision node.
    Player current_player(const State& state) const;
    // Legal actions at a decision node, in table column order.
    FixedVector<Action, kMaxActions> legal_actions(const State& state) const;
    // Returns the state after a betting action or a dealt card.
    State apply_action(const State& state, Action action) const;
    // Chips won by player at a terminal state; the game is zero-sum.
    double terminal_utility(const State& state, Player player) const;
    // Text label of what the acting player
    // knows: cards seen and betting.
    InfosetLabel infoset_label(const State& state) const;
    // Number of GameShrink infosets, the row count of a solver table.
    std::uint32_t infoset_count() const;
    // Dense row of the acting player's infoset, below infoset_count.
    std::uint32_t infoset_index(const State& state) const;
    // Each possible deal at a chance node with its probability.
    FixedVector<ChanceOutcome, kMaxChanceOutcomes> chance_outcomes(
        const State& state) const;

private:
    // Shared partition. class_of is an array read on the hot path.
    const abstraction::RihPartition* partition_;
};

}
