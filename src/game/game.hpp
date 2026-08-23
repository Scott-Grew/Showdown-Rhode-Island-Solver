#pragma once

#include <array>
#include <concepts>
#include <cstddef>
#include <cstdint>
#include <string>
#include <utility>
#include <vector>

namespace cfr::game {

using Player = int;
using Action = int;
using InfoSetKey = std::string;

inline constexpr std::size_t kMaxHistory = 32;
inline constexpr std::size_t kMaxPublic = 2;

struct State {
    std::array<std::int8_t, 2> private_cards{-1, -1};
    std::array<std::int8_t, kMaxPublic> public_cards{-1, -1};
    std::uint8_t public_count = 0;
    std::array<std::uint8_t, kMaxHistory> history{};
    std::uint8_t history_len = 0;
};

template <typename CandidateGame>
concept GameLike = requires(const CandidateGame game, const State state, Action action, Player player) {
    { game.initial_state() } -> std::same_as<State>;
    { game.is_terminal(state) } -> std::same_as<bool>;
    { game.is_chance(state) } -> std::same_as<bool>;
    { game.current_player(state) } -> std::same_as<Player>;
    { game.legal_actions(state) } -> std::same_as<std::vector<Action>>;
    { game.apply_action(state, action) } -> std::same_as<State>;
    { game.terminal_utility(state, player) } -> std::same_as<double>;
    { game.infoset_index(state) } -> std::same_as<std::uint32_t>;
    { game.infoset_count() } -> std::same_as<std::uint32_t>;
    { game.chance_outcomes(state) } -> std::same_as<std::vector<std::pair<Action, double>>>;
    { CandidateGame::kMaxActions } -> std::convertible_to<std::size_t>;
};

template <typename CandidateGame>
concept LabelledGame = GameLike<CandidateGame> && requires(const CandidateGame game, const State state) {
    { game.infoset_label(state) } -> std::same_as<InfoSetKey>;
};

}
