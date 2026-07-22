#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include <map>
#include <set>
#include <tuple>
#include <vector>

#include "game/kuhn.hpp"
#include "tree_walk.hpp"

using namespace cfr::game;

TEST_CASE("V1: kuhn zero-sum at every terminal") {
    KuhnGame game;
    walk(game, game.initial_state(), [&](const State& s) {
        if (!game.is_terminal(s)) return;
        REQUIRE(game.terminal_utility(s, 0) + game.terminal_utility(s, 1) == 0.0);
    });
}

TEST_CASE("V8: kuhn exactly 12 infosets") {
    KuhnGame game;
    std::set<InfoSetKey> keys;
    walk(game, game.initial_state(), [&](const State& s) {
        if (!game.is_terminal(s) && !game.is_chance(s))
            keys.insert(game.infoset_key(s));
    });
    REQUIRE(keys.size() == 12);
}

TEST_CASE("V2: kuhn infoset key hides opponent card") {

    KuhnGame game;
    std::map<std::tuple<int, int, std::vector<int>>, std::set<InfoSetKey>> keys_by_group;

    walk(game, game.initial_state(), [&](const State& s) {
        if (game.is_terminal(s) || game.is_chance(s)) return;
        int player = game.current_player(s);
        int own_card = s.private_cards[player];
        std::vector<int> betting(s.history.begin() + 2, s.history.end());
        keys_by_group[{player, own_card, betting}].insert(game.infoset_key(s));
    });

    for (auto& [group, keys] : keys_by_group) {
        REQUIRE(keys.size() == 1);
    }
}

TEST_CASE("V3: kuhn legal_actions non-empty at every non-terminal, non-chance state") {
    KuhnGame game;
    walk(game, game.initial_state(), [&](const State& s) {
        if (game.is_terminal(s) || game.is_chance(s)) return;
        REQUIRE_FALSE(game.legal_actions(s).empty());
    });
}

TEST_CASE("V6: kuhn chance outcome probabilities sum to 1") {
    KuhnGame game;
    walk(game, game.initial_state(), [&](const State& s) {
        if (!game.is_chance(s)) return;
        double probability_sum = 0.0;
        for (auto& [action, probability] : game.chance_outcomes(s)) {
            probability_sum += probability;
        }
        REQUIRE_THAT(probability_sum, Catch::Matchers::WithinAbs(1.0, 1e-12));
    });
}

TEST_CASE("kuhn: J bets, K calls -> J loses 2") {
    KuhnGame game;
    State state = game.initial_state();
    state = game.apply_action(state, kKuhnJack);
    state = game.apply_action(state, kKuhnKing);
    state = game.apply_action(state, kActionBet);
    state = game.apply_action(state, kActionCall);
    REQUIRE(game.is_terminal(state));
    REQUIRE(game.terminal_utility(state, 0) == -2.0);
    REQUIRE(game.terminal_utility(state, 1) == 2.0);
}

TEST_CASE("kuhn: bet-fold pays the bettor 1") {
    KuhnGame game;
    State state = game.initial_state();
    state = game.apply_action(state, kKuhnQueen);
    state = game.apply_action(state, kKuhnKing);
    state = game.apply_action(state, kActionBet);
    state = game.apply_action(state, kActionFold);
    REQUIRE(game.is_terminal(state));
    REQUIRE(game.terminal_utility(state, 0) == 1.0);
    REQUIRE(game.terminal_utility(state, 1) == -1.0);
}
