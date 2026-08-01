#pragma once

#include <cstddef>

#include <cstdint>
#include <utility>
#include <vector>

#include "game/game.hpp"

namespace cfr::game {

constexpr int kKuhnJack = 0;
constexpr int kKuhnQueen = 1;
constexpr int kKuhnKing = 2;
constexpr int kKuhnDeckSize = 3;

constexpr Action kKuhnActionCheck = 0;
constexpr Action kKuhnActionBet = 1;
constexpr Action kKuhnActionCall = 2;
constexpr Action kKuhnActionFold = 3;

class KuhnGame {
public:
    static constexpr std::size_t kMaxActions = 2;

    State initial_state() const;
    bool is_terminal(const State& state) const;
    bool is_chance(const State& state) const;
    Player current_player(const State& state) const;
    std::vector<Action> legal_actions(const State& state) const;
    State apply_action(const State& state, Action action) const;
    double terminal_utility(const State& state, Player player) const;
    InfoSetKey infoset_label(const State& state) const;
    std::uint32_t infoset_count() const;
    std::uint32_t infoset_index(const State& state) const;
    std::vector<std::pair<Action, double>> chance_outcomes(const State& state) const;

private:

    static std::vector<Action> betting_history(const State& state);

    static double player0_utility(const State& state);

    static int betting_stage(const std::vector<Action>& betting);
};

}
