#pragma once

#include <array>
#include <vector>

#include "game/game.hpp"
#include "solver/strategy.hpp"

namespace cfr::solver {

class CfrPlus {
public:
    explicit CfrPlus(const game::Game& game);

    void run_iterations(int iteration_count);

    StrategyProfile average_strategy() const;
    StrategyProfile current_strategy() const;

    int iterations_run() const;

private:

    std::array<double, 2> traverse(const game::State& state, double player0_reach, double player1_reach,
                                    double chance_reach, game::Player updating_player);

    const game::Game& game_;
    std::vector<std::vector<double>> cumulative_regrets_;
    std::vector<std::vector<double>> regret_snapshot_;
    std::vector<std::vector<double>> strategy_sums_;
    int iteration_ = 0;
};

}
