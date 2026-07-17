#pragma once

#include "game/game.hpp"
#include "solver/strategy.hpp"

namespace cfr::solver {

// Expected value for `responder` playing an exact best response against
// opponent_strategy held fixed for the other player. A single bottom-up
// pass over the real game tree from game.initial_state() -- no iteration:
//   - terminal state: game.terminal_utility(state, responder).
//   - chance node: expectation over game.chance_outcomes(state).
//   - responder's own node: max over game.legal_actions(state).
//   - opponent's node: expectation under opponent_strategy at that
//     infoset, with probabilities indexed in game.legal_actions(state)
//     order (same convention StrategyProfile documents). An infoset
//     absent from opponent_strategy (never visited while it was built)
//     falls back to the uniform distribution over its legal actions, so
//     best response stays well-defined against partially-specified
//     profiles.
double best_response_value(const game::Game& game, const StrategyProfile& opponent_strategy,
                            game::Player responder);

// Zero-sum exploitability: the average of both players' best-response
// values against the same profile. 0 at a Nash equilibrium, > 0 otherwise
// (NashConv = best_response_value(game, profile, 0) + best_response_value(
// game, profile, 1); exploitability = NashConv / 2).
double exploitability(const game::Game& game, const StrategyProfile& profile);

}  // namespace cfr::solver
