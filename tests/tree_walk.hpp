#pragma once

#include "game/game.hpp"

namespace cfr::game {

template <typename GameT, typename Fn>
void walk(const GameT& game, const State& state, Fn&& visit) {
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

}
