#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include <algorithm>
#include <utility>
#include <vector>

#include "game/game_concept.hpp"
#include "game/rhode_island.hpp"

using namespace cfr::game;

static_assert(GameLike<RhodeIslandGame>);

TEST_CASE("rhode island satisfies GameLike", "[concept]") {
    REQUIRE(GameLike<RhodeIslandGame>);
}

TEST_CASE("rhode island: initial state is a chance node") {
    RhodeIslandGame game;
    State state = game.initial_state();
    REQUIRE(game.is_chance(state));
}

TEST_CASE("rhode island: after both hole cards dealt, player 0 acts with check or raise") {
    RhodeIslandGame game;
    State state = game.initial_state();
    state = game.apply_action(state, kRihChanceCardOffset + 0);
    state = game.apply_action(state, kRihChanceCardOffset + 4);

    REQUIRE_FALSE(game.is_chance(state));
    REQUIRE(game.current_player(state) == 0);
    std::vector<Action> legal = game.legal_actions(state);
    REQUIRE(legal.size() == 2);
    REQUIRE(std::find(legal.begin(), legal.end(), kRihActionCallCheck) != legal.end());
    REQUIRE(std::find(legal.begin(), legal.end(), kRihActionRaise) != legal.end());
}

TEST_CASE("rhode island: raise cap enforced at 3 per round") {
    RhodeIslandGame game;
    State state = game.initial_state();
    state = game.apply_action(state, kRihChanceCardOffset + 0);
    state = game.apply_action(state, kRihChanceCardOffset + 4);
    state = game.apply_action(state, kRihActionCallCheck);
    state = game.apply_action(state, kRihActionRaise);
    state = game.apply_action(state, kRihActionRaise);
    state = game.apply_action(state, kRihActionRaise);

    REQUIRE_FALSE(game.is_terminal(state));
    std::vector<Action> legal = game.legal_actions(state);
    REQUIRE(std::find(legal.begin(), legal.end(), kRihActionRaise) == legal.end());
    REQUIRE(std::find(legal.begin(), legal.end(), kRihActionFold) != legal.end());
    REQUIRE(std::find(legal.begin(), legal.end(), kRihActionCallCheck) != legal.end());
}

TEST_CASE("rhode island: round transitions through flop and turn to terminal") {
    RhodeIslandGame game;
    State state = game.initial_state();
    state = game.apply_action(state, kRihChanceCardOffset + 0);
    state = game.apply_action(state, kRihChanceCardOffset + 4);

    state = game.apply_action(state, kRihActionCallCheck);
    state = game.apply_action(state, kRihActionCallCheck);
    REQUIRE(game.is_chance(state));

    std::vector<std::pair<Action, double>> flop_outcomes = game.chance_outcomes(state);
    state = game.apply_action(state, flop_outcomes.front().first);
    REQUIRE_FALSE(game.is_chance(state));
    REQUIRE_FALSE(game.is_terminal(state));
    REQUIRE(game.current_player(state) == 0);

    state = game.apply_action(state, kRihActionCallCheck);
    state = game.apply_action(state, kRihActionCallCheck);
    REQUIRE(game.is_chance(state));

    std::vector<std::pair<Action, double>> turn_outcomes = game.chance_outcomes(state);
    state = game.apply_action(state, turn_outcomes.front().first);
    REQUIRE_FALSE(game.is_chance(state));
    REQUIRE_FALSE(game.is_terminal(state));
    REQUIRE(game.current_player(state) == 0);

    state = game.apply_action(state, kRihActionCallCheck);
    state = game.apply_action(state, kRihActionCallCheck);
    REQUIRE(game.is_terminal(state));
    REQUIRE(state.pot == 10);
}

TEST_CASE("rhode island: chance outcome probabilities sum to 1 and exclude dealt cards") {
    RhodeIslandGame game;
    State state = game.initial_state();
    REQUIRE(game.is_chance(state));

    std::vector<std::pair<Action, double>> hole0_outcomes = game.chance_outcomes(state);
    REQUIRE(hole0_outcomes.size() == 52);
    double hole0_probability_sum = 0.0;
    for (auto& [action, probability] : hole0_outcomes) hole0_probability_sum += probability;
    REQUIRE_THAT(hole0_probability_sum, Catch::Matchers::WithinAbs(1.0, 1e-12));

    Action hole0_dealt_action = hole0_outcomes.front().first;
    state = game.apply_action(state, hole0_dealt_action);

    std::vector<std::pair<Action, double>> hole1_outcomes = game.chance_outcomes(state);
    REQUIRE(hole1_outcomes.size() == 51);
    for (auto& [action, probability] : hole1_outcomes) REQUIRE(action != hole0_dealt_action);
    double hole1_probability_sum = 0.0;
    for (auto& [action, probability] : hole1_outcomes) hole1_probability_sum += probability;
    REQUIRE_THAT(hole1_probability_sum, Catch::Matchers::WithinAbs(1.0, 1e-12));

    state = game.apply_action(state, hole1_outcomes.front().first);
    state = game.apply_action(state, kRihActionCallCheck);
    state = game.apply_action(state, kRihActionCallCheck);
    REQUIRE(game.is_chance(state));

    std::vector<std::pair<Action, double>> flop_outcomes = game.chance_outcomes(state);
    REQUIRE(flop_outcomes.size() == 50);
    double flop_probability_sum = 0.0;
    for (auto& [action, probability] : flop_outcomes) flop_probability_sum += probability;
    REQUIRE_THAT(flop_probability_sum, Catch::Matchers::WithinAbs(1.0, 1e-12));

    Action flop_dealt_action = flop_outcomes.front().first;
    state = game.apply_action(state, flop_dealt_action);
    state = game.apply_action(state, kRihActionCallCheck);
    state = game.apply_action(state, kRihActionCallCheck);
    REQUIRE(game.is_chance(state));

    std::vector<std::pair<Action, double>> turn_outcomes = game.chance_outcomes(state);
    REQUIRE(turn_outcomes.size() == 49);
    for (auto& [action, probability] : turn_outcomes) REQUIRE(action != flop_dealt_action);
    double turn_probability_sum = 0.0;
    for (auto& [action, probability] : turn_outcomes) turn_probability_sum += probability;
    REQUIRE_THAT(turn_probability_sum, Catch::Matchers::WithinAbs(1.0, 1e-12));
}

TEST_CASE("rhode island: fold in round 1 pays the raiser the folder's contribution") {
    RhodeIslandGame game;
    State state = game.initial_state();
    state = game.apply_action(state, kRihChanceCardOffset + 0);
    state = game.apply_action(state, kRihChanceCardOffset + 4);

    state = game.apply_action(state, kRihActionCallCheck);
    state = game.apply_action(state, kRihActionRaise);
    state = game.apply_action(state, kRihActionFold);

    REQUIRE(game.is_terminal(state));
    REQUIRE(state.pot == 20);
    REQUIRE(game.terminal_utility(state, 0) == -5.0);
    REQUIRE(game.terminal_utility(state, 1) == 5.0);
}
