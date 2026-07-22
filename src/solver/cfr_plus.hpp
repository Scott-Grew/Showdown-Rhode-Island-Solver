#pragma once

#include <array>
#include <map>
#include <vector>

#include "game/game.hpp"
#include "solver/strategy.hpp"

namespace cfr::solver {

// CFR+ (Tammelin 2014): alternating single-player updates, regret-matching+
// (floor cumulative regret at 0 in place, every update -- see
// accumulate_regret_plus), and linear averaging of the strategy sum. Same
// exact full-tree recursive traversal as VanillaCfr -- no sampling, no seed,
// fully deterministic by construction -- but differs from vanilla in three
// ways:
//   - Alternating updates: each run_iterations() step picks a single
//     updating_player = iteration_ % 2 and only that player's cumulative
//     regrets and strategy-sum get written this traversal (vanilla writes
//     both players' every traversal).
//   - Regret-matching+: increments are floored to 0 in place immediately
//     (accumulate_regret_plus), not just clipped to positive at read time
//     the way vanilla's regret_matching_strategy does -- CFR+ forgets
//     negative regret history rather than merely ignoring it when reading.
//   - Linear averaging: strategy-sum increments are weighted by iteration_
//     itself (Tammelin's default CFR+ variant), not uniformly per iteration
//     the way vanilla weights them -- later iterations count more toward
//     the average strategy.
//
// Single sigma^t per iteration, still: alternating single-player updates
// does NOT remove vanilla's multi-visit-per-traversal hazard
// (regret-ordering, see VanillaCfr's docblock and STATUS.md's
// REGRET-ORDERING entry) -- updating_player's own infosets can still recur
// more than once in one traversal (e.g. once per opponent private card),
// and every one of those visits must see the same iteration-t profile or
// the convergence argument isn't about a single well-defined profile at
// all. traverse() therefore reads strategies from regret_snapshot_, a copy
// of cumulative_regrets_ taken at the start of the iteration, exactly as
// vanilla does, while regret increments are written to the live
// cumulative_regrets_. The non-updating player's live table is untouched
// this iteration regardless (snapshot == live for them), so one shared
// snapshot correctly serves both players' strategy reads.
class CfrPlus {
public:
    explicit CfrPlus(const game::Game& game);

    void run_iterations(int iteration_count);          // alternates updating player internally

    StrategyProfile average_strategy() const;           // strategy-sum normalized, linear weighting
    StrategyProfile current_strategy() const;            // regret-matched, diagnostics

    int iterations_run() const;

private:
    // One full-tree recursive pass from `state`, given both players' reach
    // probabilities and chance's reach so far, and which player updates
    // this iteration. Regret-matched strategies are read from
    // regret_snapshot_ (this iteration's fixed sigma^t) for both players;
    // regret increments and strategy-sum increments are written to the
    // live tables only for updating_player's infosets. Returns {player 0's
    // expected utility, player 1's expected utility} from `state` onward
    // under sigma^t.
    std::array<double, 2> traverse(const game::State& state, double player0_reach, double player1_reach,
                                    double chance_reach, game::Player updating_player);

    const game::Game& game_;
    std::map<game::InfoSetKey, std::vector<double>> cumulative_regrets_;
    std::map<game::InfoSetKey, std::vector<double>> regret_snapshot_;
    std::map<game::InfoSetKey, std::vector<double>> strategy_sums_;
    int iteration_ = 0;
};

}  // namespace cfr::solver
