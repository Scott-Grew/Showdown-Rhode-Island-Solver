#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <utility>
#include <vector>

#include "game/betting_round.hpp"
#include "game/game.hpp"

namespace cfr::game {

constexpr int kLeducDeckSize = 6;

constexpr int kRound1Bet = 2;
constexpr int kRound2Bet = 4;
constexpr int kMaxRaisesPerRound = 2;
constexpr int kLeducAnte = 1;

std::array<int, 2> leduc_contributions(const State& state);

class LeducGame {
public:
    static constexpr std::size_t kMaxActions = 3;

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
};

}
