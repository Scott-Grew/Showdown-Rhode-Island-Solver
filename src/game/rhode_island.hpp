#pragma once

#include <cstddef>

#include <array>
#include <cstdint>
#include <string>
#include <utility>
#include <vector>

#include "game/betting_round.hpp"
#include "game/game.hpp"

namespace cfr::game {

constexpr int kRihDeckSize = 52;

constexpr int kRihRound1Bet = 10;
constexpr int kRihRound2Bet = 20;
constexpr int kRihRound3Bet = 20;
constexpr int kRihMaxRaisesPerRound = 3;
constexpr int kRihAnte = 5;

std::array<int, 2> rih_contributions(const State& state);

std::string rih_card_name(int card);

class RhodeIslandGame {
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
