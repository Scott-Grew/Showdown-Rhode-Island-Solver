// Leduc: infoset rows, hidden cards, the raise cap, and chip accounting.

#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <cstdint>

#include "oracles/leduc.hpp"
#include "tree_walk.hpp"

using namespace cfr::game;

TEST_CASE("leduc infoset index is a bijection onto 288 rows") {
    LeducGame game;
    CHECK(require_infoset_bijection(game) == 288);
}

TEST_CASE("leduc chance outcome probabilities sum to 1") {
    LeducGame game;
    require_chance_distribution(game);
}

TEST_CASE("leduc: raise cap enforced at 2 per round") {
    LeducGame game;
    State state = game.initial_state();
    state = game.apply_action(state, kChanceCardOffset + 0);
    state = game.apply_action(state, kChanceCardOffset + 2);
    state = game.apply_action(state, kActionCallCheck);
    state = game.apply_action(state, kActionRaise);
    state = game.apply_action(state, kActionRaise);

    REQUIRE_FALSE(game.is_terminal(state));
    auto legal = game.legal_actions(state);
    REQUIRE(std::find(legal.begin(), legal.end(), kActionRaise) == legal.end());
    REQUIRE(std::find(legal.begin(), legal.end(), kActionFold) != legal.end());
    REQUIRE(std::find(legal.begin(), legal.end(), kActionCallCheck) !=
            legal.end());
}

TEST_CASE("leduc: split pot pays 0 to both players") {
    LeducGame game;
    State state = game.initial_state();

    state = game.apply_action(state, kChanceCardOffset + 2);
    state = game.apply_action(state, kChanceCardOffset + 3);
    state = game.apply_action(state, kActionCallCheck);
    state = game.apply_action(state, kActionCallCheck);
    REQUIRE(game.is_chance(state));
    state = game.apply_action(state, kChanceCardOffset + 4);
    state = game.apply_action(state, kActionCallCheck);
    state = game.apply_action(state, kActionCallCheck);

    REQUIRE(game.is_terminal(state));
    REQUIRE(game.terminal_utility(state, 0) == 0.0);
    REQUIRE(game.terminal_utility(state, 1) == 0.0);
}

TEST_CASE("leduc: round 2 bet doubles round 1's size") {
    LeducGame game;
    State state = game.initial_state();

    state = game.apply_action(state, kChanceCardOffset + 4);
    state = game.apply_action(state, kChanceCardOffset + 0);
    state = game.apply_action(state, kActionCallCheck);
    state = game.apply_action(state, kActionCallCheck);
    state = game.apply_action(state, kChanceCardOffset + 2);
    state = game.apply_action(state, kActionRaise);
    state = game.apply_action(state, kActionCallCheck);

    REQUIRE(game.is_terminal(state));
    REQUIRE(game.terminal_utility(state, 0) == 5.0);
    REQUIRE(game.terminal_utility(state, 1) == -5.0);
}

TEST_CASE("leduc re-raise line pot is exact at every step") {
    LeducGame game;
    auto pot_of = [](const State& state) {
        return state.contribution[0] + state.contribution[1];
    };
    State state = game.initial_state();
    state = game.apply_action(state, kChanceCardOffset + 0);
    state = game.apply_action(state, kChanceCardOffset + 2);
    REQUIRE(pot_of(state) == 2);
    state = game.apply_action(state, kActionRaise);
    REQUIRE(pot_of(state) == 4);
    state = game.apply_action(state, kActionRaise);
    REQUIRE(pot_of(state) == 8);
    state = game.apply_action(state, kActionCallCheck);
    REQUIRE(pot_of(state) == 10);
}

TEST_CASE("leduc showdown pays half the pot") {
    LeducGame game;
    require_showdown_pot(game, game.initial_state(),
                         [](const State& state) { return state.contribution; });
}
