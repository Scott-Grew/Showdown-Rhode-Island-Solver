#pragma once

#include <string>
#include <utility>
#include <vector>

namespace cfr::game {

using Player = int;                    // 0, 1; chance = -1
using Action = int;                    // game-defined encoding
using InfoSetKey = std::string;        // M0: readable; packed key = later latitude

// State: value-semantic tagged container, game interprets contents.
struct State {
    std::vector<int> private_cards;   // per player; -1 = undealt
    std::vector<int> public_cards;
    std::vector<Action> history;      // actions incl. chance outcomes as dealt-card ids
    int pot = 0;
};

// Node-walk contract; the solver traverses the game tree via these methods
// alone. Virtual dispatch is fine at M0 (correctness reference); CRTP/template
// latitude is open for M1+ if it turns out to be hot — the contract below is
// what's pinned, not the dispatch mechanism.
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
    virtual InfoSetKey infoset_key(const State& state) const = 0;
    virtual std::vector<std::pair<Action, double>> chance_outcomes(const State& state) const = 0;
};

}  // namespace cfr::game
