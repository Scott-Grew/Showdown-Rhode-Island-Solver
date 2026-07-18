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
// Single sigma^t per iteration: an infoset can be visited more than once
// per traversal (e.g. once per opponent private card in Kuhn), and every
// one of those visits must see the *same* regret-matched strategy -- the
// iteration-t profile sigma^t -- or the traversal isn't sampling a single
// well-defined profile at all, and the Zinkevich 2007 regret bound (which
// is stated in terms of one sigma^t per iteration) is never actually
// established. traverse() therefore reads strategies from regret_snapshot_,
// a copy of cumulative_regrets_ taken at the start of the iteration, while
// regret increments are written to the live cumulative_regrets_; the next
// iteration's snapshot then picks up everything accumulated so far. This
// costs one table copy per iteration, paid deliberately so this solver
// keeps proving the profile the convergence guarantee is proved for -- it
// is the permanent correctness reference other solvers (M3+) get checked
// against, so this isn't a place to cut the corner for speed.
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
    // probabilities and chance's reach so far. Regret-matched strategies are
    // read from regret_snapshot_ (this iteration's fixed sigma^t); regret
    // increments are written to the live cumulative_regrets_. Updates
    // strategy_sums_ for every player infoset visited along the way; returns
    // {player 0's expected utility, player 1's expected utility} from
    // `state` onward under sigma^t.
    std::array<double, 2> traverse(const game::State& state, double player0_reach, double player1_reach,
                                    double chance_reach);

    const game::Game& game_;
    std::map<game::InfoSetKey, std::vector<double>> cumulative_regrets_;
    std::map<game::InfoSetKey, std::vector<double>> regret_snapshot_;
    std::map<game::InfoSetKey, std::vector<double>> strategy_sums_;
};

}  // namespace cfr::solver
