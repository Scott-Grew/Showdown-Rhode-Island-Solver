#pragma once

#include <concepts>
#include <cstdint>
#include <utility>
#include <vector>

#include "game/game.hpp"

namespace cfr::game {

template <typename CandidateGame>
concept GameLike = requires(const CandidateGame game, const State state, Action action, Player player) {
    { game.initial_state() } -> std::same_as<State>;
    { game.is_terminal(state) } -> std::same_as<bool>;
    { game.is_chance(state) } -> std::same_as<bool>;
    { game.current_player(state) } -> std::same_as<Player>;
    { game.legal_actions(state) } -> std::same_as<std::vector<Action>>;
    { game.apply_action(state, action) } -> std::same_as<State>;
    { game.terminal_utility(state, player) } -> std::same_as<double>;
    { game.infoset_label(state) } -> std::same_as<InfoSetKey>;
    { game.infoset_index(state) } -> std::same_as<std::uint32_t>;
    { game.infoset_count() } -> std::same_as<std::uint32_t>;
    { game.chance_outcomes(state) } -> std::same_as<std::vector<std::pair<Action, double>>>;
};

}
