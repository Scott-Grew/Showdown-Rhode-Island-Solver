#include "solver/best_response.hpp"

#include <algorithm>
#include <cstddef>
#include <limits>

namespace cfr::solver {

namespace {

double best_response_recursive(const game::Game& game, const game::State& state,
                                const StrategyProfile& opponent_strategy, game::Player responder) {
    if (game.is_terminal(state)) {
        return game.terminal_utility(state, responder);
    }

    if (game.is_chance(state)) {
        double expected_value = 0.0;
        for (auto& [action, probability] : game.chance_outcomes(state)) {
            expected_value += probability * best_response_recursive(game, game.apply_action(state, action),
                                                                      opponent_strategy, responder);
        }
        return expected_value;
    }

    std::vector<game::Action> actions = game.legal_actions(state);

    if (game.current_player(state) == responder) {
        double best_value = -std::numeric_limits<double>::infinity();
        for (game::Action action : actions) {
            best_value = std::max(best_value, best_response_recursive(game, game.apply_action(state, action),
                                                                        opponent_strategy, responder));
        }
        return best_value;
    }

    // Opponent's node: expectation under their fixed strategy. An infoset
    // missing from the profile falls back to uniform (documented in the
    // header) so best response stays defined against partial profiles.
    auto profile_entry = opponent_strategy.find(game.infoset_key(state));
    double uniform_probability = 1.0 / static_cast<double>(actions.size());

    double expected_value = 0.0;
    for (std::size_t i = 0; i < actions.size(); ++i) {
        double action_probability =
            profile_entry != opponent_strategy.end() ? profile_entry->second[i] : uniform_probability;
        expected_value += action_probability * best_response_recursive(game, game.apply_action(state, actions[i]),
                                                                         opponent_strategy, responder);
    }
    return expected_value;
}

}  // namespace

double best_response_value(const game::Game& game, const StrategyProfile& opponent_strategy,
                            game::Player responder) {
    return best_response_recursive(game, game.initial_state(), opponent_strategy, responder);
}

double exploitability(const game::Game& game, const StrategyProfile& profile) {
    return (best_response_value(game, profile, 0) + best_response_value(game, profile, 1)) / 2.0;
}

}  // namespace cfr::solver
