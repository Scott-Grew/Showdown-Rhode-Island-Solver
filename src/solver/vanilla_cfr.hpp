#pragma once

#include <array>
#include <map>
#include <vector>

#include "game/game.hpp"
#include "solver/strategy.hpp"

namespace cfr::solver {

// Vanilla CFR (Zinkevich et al. 2007): exact, full-tree recursive
// traversal -- no sampling, no seed, fully deterministic by construction.
// Each run_iterations() step performs one full-tree pass that updates BOTH
// players' cumulative regrets in the same traversal (the "simultaneous"
// vanilla form, as opposed to alternating single-player passes): at each
// acting player's infoset, regret is accumulated weighted by counterfactual
// reach (the *opponent's* reach probability times chance's reach), and the
// infoset's strategy-sum is accumulated weighted by the acting player's own
// reach. average_strategy() -- the time-averaged strategy -- is what CFR's
// convergence guarantee actually applies to; current_strategy() (the
// regret-matched strategy at the most recent iteration) does not converge
// and exists for diagnostics only.
//
// This is deliberately the slow, exact correctness reference (blueprint
// risk table): it stays in the codebase forever, unoptimized, as the
// yardstick sampling-based solvers (M3+) get checked against.
class VanillaCfr {
public:
    explicit VanillaCfr(const game::Game& game);

    void run_iterations(int iteration_count);         // both players updated per iter

    StrategyProfile average_strategy() const;          // strategy-sum normalized
    StrategyProfile current_strategy() const;           // regret-matched, diagnostics

private:
    // One full-tree recursive pass from `state`, given both players' reach
    // probabilities and chance's reach so far. Updates cumulative_regrets_
    // and strategy_sums_ for every player infoset visited along the way;
    // returns {player 0's expected utility, player 1's expected utility}
    // from `state` onward under the current regret-matched strategies.
    std::array<double, 2> traverse(const game::State& state, double player0_reach, double player1_reach,
                                    double chance_reach);

    const game::Game& game_;
    std::map<game::InfoSetKey, std::vector<double>> cumulative_regrets_;
    std::map<game::InfoSetKey, std::vector<double>> strategy_sums_;
};

}  // namespace cfr::solver
