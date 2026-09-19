#pragma once

#include <array>
#include <cstdint>
#include <functional>
#include <span>
#include <vector>

#include "game/card.hpp"
#include "game/game.hpp"

namespace cfr::solver {

// One value per card id, read as the responder's or the opponent's
// possible hole card.
using RihCardVector = std::array<double, kCardCount>;

// Fills probabilities_by_card[card * action_count + action] with
// the actor's strategy at state for every hole card.
using RihStrategyQuery =
    std::function<void(const game::State&, game::Player actor, std::vector<double>& probabilities_by_card)>;

// Expected chips per hand the responder wins with a best
// response to opponent_strategy.
double rih_best_response_value(const RihStrategyQuery& opponent_strategy, game::Player responder);

// Expected chips per hand for player when both sides follow
// profile.
double rih_strategy_value(const RihStrategyQuery& profile, game::Player player);

// As rih_best_response_value, but the responder may deviate
// only in the round with this many board cards dealt.
double rih_best_response_value_in_round(const RihStrategyQuery& opponent_strategy, game::Player responder,
                                         int round);

// Adapts a solver's average strategy to a RihStrategyQuery. The
// result holds references to source and game.
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
