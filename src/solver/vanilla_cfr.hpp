#pragma once

#include <array>
#include <map>
#include <vector>

#include "game/game.hpp"
#include "solver/strategy.hpp"

namespace cfr::solver {

class VanillaCfr {
public:
    explicit VanillaCfr(const game::Game& game);

    void run_iterations(int iteration_count);

    StrategyProfile average_strategy() const;
    StrategyProfile current_strategy() const;

private:

    std::array<double, 2> traverse(const game::State& state, double player0_reach, double player1_reach,
                                    double chance_reach);

    const game::Game& game_;
    std::map<game::InfoSetKey, std::vector<double>> cumulative_regrets_;
    std::map<game::InfoSetKey, std::vector<double>> regret_snapshot_;
    std::map<game::InfoSetKey, std::vector<double>> strategy_sums_;
};

}
