#pragma once

#include "game/game.hpp"

namespace cfr::game {

// tree_walk.hpp — exhaustive DFS; f runs at every state.
template <typename Fn>
void walk(const Game& game, const State& state, Fn&& visit) {
    visit(state);
    if (game.is_terminal(state)) return;
    if (game.is_chance(state)) {
        for (auto& [action, prob] : game.chance_outcomes(state))
            walk(game, game.apply_action(state, action), visit);
        return;
    }
    for (Action action : game.legal_actions(state))
        walk(game, game.apply_action(state, action), visit);
}

}  // namespace cfr::game
