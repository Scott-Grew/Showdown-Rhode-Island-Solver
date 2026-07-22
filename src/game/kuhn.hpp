#pragma once

#include <cstdint>
#include <utility>
#include <vector>

#include "game/game.hpp"

namespace cfr::game {

constexpr int kKuhnJack = 0;
constexpr int kKuhnQueen = 1;
constexpr int kKuhnKing = 2;
constexpr int kKuhnDeckSize = 3;

constexpr Action kActionCheck = 0;
constexpr Action kActionBet = 1;
constexpr Action kActionCall = 2;
constexpr Action kActionFold = 3;

class KuhnGame : public Game {
public:
    State initial_state() const override;
    bool is_terminal(const State& state) const override;
    bool is_chance(const State& state) const override;
    Player current_player(const State& state) const override;
    std::vector<Action> legal_actions(const State& state) const override;
    State apply_action(const State& state, Action action) const override;
    double terminal_utility(const State& state, Player player) const override;
    InfoSetKey infoset_label(const State& state) const override;
    std::uint32_t infoset_count() const override;
    std::uint32_t infoset_index(const State& state) const override;
    std::vector<std::pair<Action, double>> chance_outcomes(const State& state) const override;

private:

    static std::vector<Action> betting_history(const State& state);

    static double player0_utility(const State& state);

    static int betting_stage(const std::vector<Action>& betting);
};

}
