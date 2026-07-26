#pragma once

#include <cstddef>

#include <cstdint>
#include <utility>
#include <vector>

#include "game/game.hpp"

namespace cfr::game {

constexpr int kRihDeckSize = 52;

constexpr Action kRihActionFold = 0;
constexpr Action kRihActionCallCheck = 1;
constexpr Action kRihActionRaise = 2;

constexpr int kRihRound1Bet = 10;
constexpr int kRihRound2Bet = 20;
constexpr int kRihRound3Bet = 20;
constexpr int kRihMaxRaisesPerRound = 3;
constexpr int kRihAnte = 5;

constexpr Action kRihChanceCardOffset = 100;

class RhodeIslandGame final : public Game {
public:
    static constexpr std::size_t kMaxActions = 3;

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
};

}
