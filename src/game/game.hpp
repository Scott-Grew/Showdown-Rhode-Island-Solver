#pragma once

#include <array>
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
    std::int32_t pot = 0;
};

class Game {
public:
    virtual ~Game() = default;
    virtual State initial_state() const = 0;
    virtual bool is_terminal(const State& state) const = 0;
    virtual bool is_chance(const State& state) const = 0;
    virtual Player current_player(const State& state) const = 0;
    virtual std::vector<Action> legal_actions(const State& state) const = 0;
    virtual State apply_action(const State& state, Action action) const = 0;
    virtual double terminal_utility(const State& state, Player player) const = 0;
    virtual InfoSetKey infoset_label(const State& state) const = 0;
    virtual std::uint32_t infoset_count() const = 0;
    virtual std::uint32_t infoset_index(const State& state) const = 0;
    virtual std::vector<std::pair<Action, double>> chance_outcomes(const State& state) const = 0;
};

}
