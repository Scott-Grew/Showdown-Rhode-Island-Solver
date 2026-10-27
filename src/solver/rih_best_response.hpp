// Best response for Rhode Island. It walks the public tree once with
// a value per hole card instead of visiting each deal, and reads
// strategies through the RihStrategyQuery callback so it can grade
// ours or the published one. Every value is in chips per hand,
// averaged over all ordered deals of two hole and two board cards.

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
// the acting player's strategy at state for every hole card.
using RihStrategyQuery =
    std::function<void(const game::State&, game::Player acting_player,
                       std::vector<double>& probabilities_by_card)>;

// Expected chips per hand the responder wins with a best
// response to opponent_strategy.
double rih_best_response_value(const RihStrategyQuery& opponent_strategy,
                               game::Player responder);

// Expected chips per hand for player when both seats play profile.
double rih_strategy_value(const RihStrategyQuery& profile, game::Player player);

// As rih_best_response_value, but the responder may deviate
// only in the round with this many board cards dealt.
double rih_best_response_value_in_round(
    const RihStrategyQuery& opponent_strategy, game::Player responder,
    int round);

// Best-response values of both players and their mean, the
// exploitability.
struct RihExploitability {
    double best_response_player0;
    double best_response_player1;
    double exploitability;
};

// Best response of each player when both seats play strategy. The
// mean of the two is how far strategy is from a Nash equilibrium.
inline RihExploitability rih_exploitability(const RihStrategyQuery& strategy) {
    RihExploitability report{};
    report.best_response_player0 = rih_best_response_value(strategy, 0);
    report.best_response_player1 = rih_best_response_value(strategy, 1);
    report.exploitability =
        (report.best_response_player0 + report.best_response_player1) / 2.0;
    return report;
}

// Adapts a solver's average strategy to a RihStrategyQuery. The
// result holds references to source and game.
template <typename AverageStrategySource, typename GameT>
    requires game::GameLike<GameT>
RihStrategyQuery rih_average_strategy_query(const AverageStrategySource& source,
                                            const GameT& game) {
    return [&source, &game](const game::State& state,
                            game::Player acting_player,
                            std::vector<double>& probabilities_by_card) {
        std::size_t action_count = game.legal_actions(state).size();
        probabilities_by_card.assign(
            static_cast<std::size_t>(kCardCount) * action_count, 0.0);

        game::State probe = state;
        for (int card = 0; card < kCardCount; ++card) {
            if constexpr (requires {
                              game.set_hole_card(probe, acting_player, card);
                          }) {
                game.set_hole_card(probe, acting_player, card);
            } else {
                probe.hole_cards[acting_player] =
                    static_cast<std::int8_t>(card);
            }
            source.average_strategy_into(
                game.infoset_index(probe), action_count,
                std::span<double>(
                    probabilities_by_card.data() +
                        static_cast<std::size_t>(card) * action_count,
                    action_count));
        }
    };
}

}
