#pragma once

#include "game/game.hpp"
#include "solver/strategy.hpp"

namespace cfr::solver {

double best_response_value(const game::Game& game, const StrategyProfile& opponent_strategy,
                            game::Player responder);

double exploitability(const game::Game& game, const StrategyProfile& profile);

}
