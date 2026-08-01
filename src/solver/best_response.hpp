#pragma once

#include "game/game.hpp"
#include "game/game_concept.hpp"
#include "solver/strategy.hpp"

namespace cfr::solver {

template <typename GameT>
requires game::LabelledGame<GameT>
double best_response_value(const GameT& game, const StrategyProfile& opponent_strategy, game::Player responder);

template <typename GameT>
requires game::LabelledGame<GameT>
double exploitability(const GameT& game, const StrategyProfile& profile);

}
