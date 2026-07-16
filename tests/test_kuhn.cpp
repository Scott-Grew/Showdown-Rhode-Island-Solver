#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include <map>
#include <set>
#include <tuple>
#include <vector>

#include "game/kuhn.hpp"
#include "tree_walk.hpp"

using namespace cfr::game;

TEST_CASE("kuhn: zero-sum at every terminal") {            // V1
    KuhnGame game;
    walk(game, game.initial_state(), [&](const State& s) {
        if (!game.is_terminal(s)) return;
        REQUIRE(game.terminal_utility(s, 0) + game.terminal_utility(s, 1) == 0.0);
    });
}

TEST_CASE("kuhn: exactly 12 infosets") {                   // V8
    KuhnGame game;
    std::set<InfoSetKey> keys;
    walk(game, game.initial_state(), [&](const State& s) {
        if (!game.is_terminal(s) && !game.is_chance(s))
            keys.insert(game.infoset_key(s));
    });
    REQUIRE(keys.size() == 12);
}

TEST_CASE("kuhn: infoset key hides opponent card") {        // V2
    // states equal in own card + history but different opponent card → same key
    // build by walking, group by (current_player, own card, history), assert 1 key per group
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

TEST_CASE("kuhn: legal_actions non-empty at every non-terminal, non-chance state") {  // V3
    KuhnGame game;
    walk(game, game.initial_state(), [&](const State& s) {
        if (game.is_terminal(s) || game.is_chance(s)) return;
        REQUIRE_FALSE(game.legal_actions(s).empty());
    });
}

TEST_CASE("kuhn: chance outcome probabilities sum to 1") {  // V6
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
    state = game.apply_action(state, kKuhnJack);    // deal J to player 0
    state = game.apply_action(state, kKuhnKing);     // deal K to player 1
    state = game.apply_action(state, kActionBet);    // player 0 bets
    state = game.apply_action(state, kActionCall);   // player 1 calls
    REQUIRE(game.is_terminal(state));
    REQUIRE(game.terminal_utility(state, 0) == -2.0);
    REQUIRE(game.terminal_utility(state, 1) == 2.0);
}

TEST_CASE("kuhn: bet-fold pays the bettor 1") {
    KuhnGame game;
    State state = game.initial_state();
    state = game.apply_action(state, kKuhnQueen);   // deal Q to player 0
    state = game.apply_action(state, kKuhnKing);    // deal K to player 1
    state = game.apply_action(state, kActionBet);   // player 0 bets
    state = game.apply_action(state, kActionFold);  // player 1 folds
    REQUIRE(game.is_terminal(state));
    REQUIRE(game.terminal_utility(state, 0) == 1.0);
    REQUIRE(game.terminal_utility(state, 1) == -1.0);
}
