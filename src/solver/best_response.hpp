#pragma once

#include "game/game.hpp"
#include "solver/strategy.hpp"

namespace cfr::solver {

// Expected value for `responder` playing an exact best response against
// opponent_strategy held fixed for the other player. Two passes over the
// real game tree from game.initial_state() -- no iteration:
//   - terminal state: game.terminal_utility(state, responder).
//   - chance node: expectation over game.chance_outcomes(state).
//   - opponent's node: expectation under opponent_strategy at that
//     infoset, with probabilities indexed in game.legal_actions(state)
//     order (same convention StrategyProfile documents). An infoset
//     absent from opponent_strategy (never visited while it was built)
//     falls back to the uniform distribution over its legal actions, so
//     best response stays well-defined against partially-specified
//     profiles.
//   - responder's own node: max over game.legal_actions(state) -- but the
//     max is taken PER INFOSET, not per tree node. Two different game
//     tree nodes can share an infoset key (e.g. same responder card,
//     same betting history, different hidden opponent card) precisely
//     because the responder can't see what distinguishes them -- that's
//     what "infoset" means -- so the responder must play identically at
//     both. A first pass collects every node belonging to each of the
//     responder's infosets, weighted by the reach probability
//     contributed by chance and the *opponent* alone (never by the
//     responder's own actions -- those are exactly what's being solved
//     for). A second pass then picks, per infoset, the single action
//     maximizing the reach-weighted sum of continuation values across
//     every node sharing that infoset, and reuses that decision (memoized)
//     everywhere the infoset recurs. Maxing per tree node instead of per
//     infoset silently lets the responder react to the opponent's hidden
//     card -- a real bug found empirically in an earlier version of this
//     file: it inflated exploitability of an exact Kuhn Nash equilibrium
//     profile to ~0.28 instead of ~0.
double best_response_value(const game::Game& game, const StrategyProfile& opponent_strategy,
                            game::Player responder);

// Zero-sum exploitability: the average of both players' best-response
// values against the same profile. 0 at a Nash equilibrium, > 0 otherwise
// (NashConv = best_response_value(game, profile, 0) + best_response_value(
// game, profile, 1); exploitability = NashConv / 2).
double exploitability(const game::Game& game, const StrategyProfile& profile);

}  // namespace cfr::solver
