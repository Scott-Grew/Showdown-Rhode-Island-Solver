// Leduc rules: zero-sum payoffs, infoset indexing, hidden opponent
// card and exact pot accounting.

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include <algorithm>
#include <cstdint>
#include <map>
#include <set>
#include <tuple>
#include <vector>

#include "game/leduc.hpp"
#include "tree_walk.hpp"

using namespace cfr::game;

TEST_CASE("leduc zero-sum at every terminal") {
    LeducGame game;
    walk(game, game.initial_state(), [&](const State& s) {
        if (!game.is_terminal(s)) return;
        REQUIRE(game.terminal_utility(s, 0) + game.terminal_utility(s, 1) ==
                0.0);
    });
}

TEST_CASE("leduc infoset count is measured and stable") {
    LeducGame game;
    std::set<InfosetLabel> keys;
    walk(game, game.initial_state(), [&](const State& s) {
        if (!game.is_terminal(s) && !game.is_chance(s))
            keys.insert(game.infoset_label(s));
    });
    CHECK(keys.size() == 288);
}

TEST_CASE("leduc infoset label hides opponent card") {
    LeducGame game;
    std::map<
        std::tuple<int, int, int, std::vector<Action>, std::vector<Action>>,
        std::set<InfosetLabel>>
        keys_by_group;

    walk(game, game.initial_state(), [&](const State& s) {
        if (game.is_terminal(s) || game.is_chance(s)) return;
        int player = game.current_player(s);
        int own_card = s.hole_cards[player];
        int board_card = s.board_count == 0 ? -1 : s.board_cards[0];

        std::vector<Action> round1_betting_actions;
        std::vector<Action> round2_betting_actions;
        int chance_events_seen = 0;
        for (std::uint8_t index = 0; index < s.history_len; ++index) {
            Action entry = s.history[index];
            if (entry >= kChanceCardOffset) {
                ++chance_events_seen;
                continue;
            }
            if (chance_events_seen < 3) {
                round1_betting_actions.push_back(entry);
            } else {
                round2_betting_actions.push_back(entry);
            }
        }
        keys_by_group[{player, own_card, board_card, round1_betting_actions,
                       round2_betting_actions}]
            .insert(game.infoset_label(s));
    });

    for (auto& [group, keys] : keys_by_group) {
        REQUIRE(keys.size() == 1);
    }
}

TEST_CASE(
    "leduc legal_actions non-empty at every non-terminal, non-chance state") {
    LeducGame game;
    walk(game, game.initial_state(), [&](const State& s) {
        if (game.is_terminal(s) || game.is_chance(s)) return;
        REQUIRE_FALSE(game.legal_actions(s).empty());
    });
}

TEST_CASE("leduc chance outcome probabilities sum to 1") {
    LeducGame game;
    walk(game, game.initial_state(), [&](const State& s) {
        if (!game.is_chance(s)) return;
        double probability_sum = 0.0;
        for (auto& [action, probability] : game.chance_outcomes(s)) {
            probability_sum += probability;
        }
        REQUIRE_THAT(probability_sum, Catch::Matchers::WithinAbs(1.0, 1e-12));
    });
}

TEST_CASE("leduc infoset index is a bijection onto 0 up to infoset_count") {
    LeducGame game;
    std::map<InfosetLabel, std::uint32_t> index_by_label;
    std::set<std::uint32_t> distinct_indices;
    walk(game, game.initial_state(), [&](const State& s) {
        if (game.is_terminal(s) || game.is_chance(s)) return;
        InfosetLabel label = game.infoset_label(s);
        std::uint32_t index = game.infoset_index(s);
        REQUIRE(index < game.infoset_count());
        auto existing = index_by_label.emplace(label, index).first;
        REQUIRE(existing->second == index);
        distinct_indices.insert(index);
    });
    REQUIRE(index_by_label.size() == distinct_indices.size());
    REQUIRE(distinct_indices.size() == game.infoset_count());
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
    std::vector<Action> legal = game.legal_actions(state);
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
        std::array<int, 2> contributions = leduc_contributions(state);
        return contributions[0] + contributions[1];
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

TEST_CASE(
    "leduc showdown pays half the pot: a closed round leaves equal money in") {
    LeducGame game;
    long long showdown_terminals = 0;
    walk(game, game.initial_state(), [&](const State& state) {
        if (!game.is_terminal(state)) return;
        if (state.history[state.history_len - 1] == kActionFold) return;
        ++showdown_terminals;
        std::array<int, 2> contributions = leduc_contributions(state);
        REQUIRE(contributions[0] == contributions[1]);
        int pot = contributions[0] + contributions[1];
        REQUIRE(pot % 2 == 0);
        double first_player_utility = game.terminal_utility(state, 0);
        double half_pot = pot / 2.0;
        REQUIRE((first_player_utility == 0.0 ||
                 first_player_utility == half_pot ||
                 first_player_utility == -half_pot));
    });
    REQUIRE(showdown_terminals > 0);
}
