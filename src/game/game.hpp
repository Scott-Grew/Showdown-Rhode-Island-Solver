#pragma once

#include <cstdint>
#include <string>
#include <utility>
#include <vector>

namespace cfr::game {

using Player = int;
using Action = int;
using InfoSetKey = std::string;

struct State {
    std::vector<int> private_cards;
    std::vector<int> public_cards;
    std::vector<Action> history;
    int pot = 0;
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
