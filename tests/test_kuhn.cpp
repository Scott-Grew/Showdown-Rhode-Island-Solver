// Kuhn: 12 infosets, hidden cards, and the analytic chip payoffs.
// Decision nodes of Leduc and Rhode Island are checked here too.

#include <catch2/catch_test_macros.hpp>

#include "game/card.hpp"
#include "game/rhode_island.hpp"
#include "oracles/kuhn.hpp"
#include "oracles/leduc.hpp"
#include "tree_walk.hpp"

using namespace cfr::game;

TEST_CASE("decision nodes have a legal action") {
    KuhnGame kuhn;
    require_decisions_actionable(kuhn, kuhn.initial_state());
    LeducGame leduc;
    require_decisions_actionable(leduc, leduc.initial_state());

    // One fixed deal. Betting does not depend on the cards, which is
    // why the sequence-form integer 91,224,226 pins that topology.
    RhodeIslandGame rhode;
    State dealt = rhode.initial_state();
    dealt = rhode.apply_action(dealt, kChanceCardOffset + cfr::make_card(0, 0));
    dealt = rhode.apply_action(dealt, kChanceCardOffset + cfr::make_card(1, 0));
    require_decisions_actionable(rhode, dealt);
}

TEST_CASE("kuhn infoset index is a bijection onto 12 rows") {
    KuhnGame game;
    REQUIRE(require_infoset_bijection(game) == 12);
}

TEST_CASE("kuhn chance outcome probabilities sum to 1") {
    KuhnGame game;
    require_chance_distribution(game);
}

TEST_CASE("kuhn showdown and fold pay the analytic amounts") {
    struct Line {
        int first;
        int second;
        Action bet;
        Action answer;
        double utility0;
    };
    // J bets into K and calls: the caller loses the 2-chip pot.
    // A bet that folds wins the 1-chip ante.
    Line lines[] = {
        {kKuhnJack, kKuhnKing, kKuhnActionBet, kKuhnActionCall, -2.0},
        {kKuhnQueen, kKuhnKing, kKuhnActionBet, kKuhnActionFold, 1.0},
    };
    KuhnGame game;
    for (const Line& line : lines) {
        State state = game.initial_state();
        state = game.apply_action(state, line.first);
        state = game.apply_action(state, line.second);
        state = game.apply_action(state, line.bet);
        state = game.apply_action(state, line.answer);
        REQUIRE(game.is_terminal(state));
        REQUIRE(game.terminal_utility(state, 0) == line.utility0);
    }
}
