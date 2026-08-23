#pragma once

#include <array>
#include <cstdint>
#include <functional>
#include <span>
#include <vector>

#include "game/card.hpp"
#include "game/game.hpp"

namespace cfr::solver {

using RihCardVector = std::array<double, kCardCount>;

using RihStrategyQuery =
    std::function<void(const game::State&, game::Player actor, std::vector<double>& probabilities_by_card)>;

double rih_best_response_value(const RihStrategyQuery& opponent_strategy, game::Player responder);

double rih_strategy_value(const RihStrategyQuery& profile, game::Player player);

double rih_best_response_value_in_round(const RihStrategyQuery& opponent_strategy, game::Player responder,
                                         int round);

template <typename AverageStrategySource, typename GameT>
requires game::GameLike<GameT>
RihStrategyQuery rih_average_strategy_query(const AverageStrategySource& source, const GameT& game) {
    return [&source, &game](const game::State& state, game::Player actor, std::vector<double>& probabilities_by_card) {
        std::size_t action_count = game.legal_actions(state).size();
        probabilities_by_card.assign(static_cast<std::size_t>(kCardCount) * action_count, 0.0);

        game::State probe = state;
        for (int card = 0; card < kCardCount; ++card) {
            probe.private_cards[actor] = static_cast<std::int8_t>(card);
            source.average_strategy_into(
                game.infoset_index(probe), action_count,
                std::span<double>(probabilities_by_card.data() + static_cast<std::size_t>(card) * action_count,
                                   action_count));
        }
    };
}

}
