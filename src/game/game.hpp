// The State all three games share and the GameLike concept the
// solvers are written against. Kuhn, Leduc and Rhode Island implement
// it; nothing here knows a specific game.

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
using InfosetLabel = std::string;

// Fixed capacities of the State arrays below.
inline constexpr std::size_t kMaxHistory = 32;
inline constexpr std::size_t kMaxBoardCards = 2;

// One node of a game tree. A card entry of -1 means not dealt yet,
// and history holds chance and betting entries in play order.
struct State {
    std::array<std::int8_t, 2> hole_cards{-1, -1};
    std::array<std::int8_t, kMaxBoardCards> board_cards{-1, -1};
    std::uint8_t board_count = 0;
    std::array<std::uint8_t, kMaxHistory> history{};
    std::uint8_t history_len = 0;
};

// One branch of a chance node: the
// dealing action and how likely it is.
struct ChanceOutcome {
    Action action;
    double probability;
};

// What current_player returns at a chance node.
inline constexpr Player kChancePlayer = -1;

// What a solver needs from a game. infoset_index must be below
// infoset_count, and equal for states the acting player cannot
// tell apart.
template <typename CandidateGame>
concept GameLike = requires(const CandidateGame game, const State state,
                            Action action, Player player) {
    { game.initial_state() } -> std::same_as<State>;
    { game.is_terminal(state) } -> std::same_as<bool>;
    { game.is_chance(state) } -> std::same_as<bool>;
    { game.current_player(state) } -> std::same_as<Player>;
    { game.legal_actions(state) } -> std::same_as<std::vector<Action>>;
    { game.apply_action(state, action) } -> std::same_as<State>;
    { game.terminal_utility(state, player) } -> std::same_as<double>;
    { game.infoset_index(state) } -> std::same_as<std::uint32_t>;
    { game.infoset_count() } -> std::same_as<std::uint32_t>;
    { game.chance_outcomes(state) } -> std::same_as<std::vector<ChanceOutcome>>;
    { CandidateGame::kMaxActions } -> std::convertible_to<std::size_t>;
};

// A game that can also name each infoset as text, which
// StrategyProfile and the tree-walking best response key on.
template <typename CandidateGame>
concept LabelledGame =
    GameLike<CandidateGame> && requires(const CandidateGame game) {
        { game.infoset_label(State{}) } -> std::same_as<InfosetLabel>;
    };

}
