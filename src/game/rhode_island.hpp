// Rhode Island hold'em, the game this project solves. Its tree is too
// large to walk whole, so it is trained by mccfr_solver.hpp and
// measured by rih_best_response.hpp.

#pragma once

#include <cstddef>
#include <cstdint>

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

// One hole card each, two board cards dealt one at a time, three rounds.
class RhodeIslandGame {
public:
    static constexpr std::size_t kMaxActions = 3;

    // The first construction builds the refined partition (the
    // labelling fixed point); every game then shares it for the process.
    RhodeIslandGame();
    State initial_state() const;
    bool is_terminal(const State& state) const;
    bool is_chance(const State& state) const;
    Player current_player(const State& state) const;
    // Column order of the solver tables: check/bet, or fold/call/raise.
    FixedVector<Action, kMaxActions> legal_actions(const State& state) const;
    State apply_action(const State& state, Action action) const;
    double terminal_utility(const State& state, Player player) const;
    std::uint32_t infoset_count() const;
    std::uint32_t infoset_index(const State& state) const;
    FixedVector<ChanceOutcome, kMaxChanceOutcomes> chance_outcomes(
        const State& state) const;
    // roll is one uniform_real_distribution<double>(0, 1) sample, the
    // same draw sample_index uses, so the checkpoint stays byte-identical.
    Action sample_chance_action(const State& state, double roll) const {
        return sample_undealt_card(state, kCardCount, roll);
    }
    // Sets the hole card and card_class for infoset_index probes; it
    // does not append to history.
    void set_hole_card(State& state, Player player, int card) const;

private:
    // class_of is an array read on the hot path. One partition per process.
    const abstraction::RihPartition* partition_;
};

}
