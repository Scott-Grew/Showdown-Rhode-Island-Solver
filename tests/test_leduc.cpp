#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include <algorithm>
#include <map>
#include <set>
#include <tuple>
#include <vector>

#include "game/leduc.hpp"
#include "tree_walk.hpp"

using namespace cfr::game;

TEST_CASE("leduc: zero-sum at every terminal") {            // V1
    LeducGame game;
    walk(game, game.initial_state(), [&](const State& s) {
        if (!game.is_terminal(s)) return;
        REQUIRE(game.terminal_utility(s, 0) + game.terminal_utility(s, 1) == 0.0);
    });
}

TEST_CASE("leduc: infoset count is measured and stable") {  // V8-equivalent for Leduc
    // 288 measured by enumeration this session (`make test`, 2026-07-16) — not
    // a literature constant. If this fires, the game logic changed; re-derive
    // and update only after confirming the change was intentional.
    LeducGame game;
    std::set<InfoSetKey> keys;
    walk(game, game.initial_state(), [&](const State& s) {
        if (!game.is_terminal(s) && !game.is_chance(s))
            keys.insert(game.infoset_key(s));
    });
    CHECK(keys.size() == 288);
}

TEST_CASE("leduc: infoset key hides opponent card") {        // V2
    // group by (current_player, own card, public card, round1 betting actions,
    // round2 betting actions — kept separate, mirroring infoset_key's own
    // round1:round2 structure, so a round-boundary difference can never be
    // mistaken for a round-internal one). Chance-deal entries are stripped out
    // so the opponent's card never enters the group key. The only thing that
    // can still differ within a group is the opponent's private card, so V2
    // requires exactly one key per group.
    LeducGame game;
    std::map<std::tuple<int, int, int, std::vector<Action>, std::vector<Action>>, std::set<InfoSetKey>>
        keys_by_group;

    walk(game, game.initial_state(), [&](const State& s) {
        if (game.is_terminal(s) || game.is_chance(s)) return;
        int player = game.current_player(s);
        int own_card = s.private_cards[player];
        int public_card = s.public_cards.empty() ? -1 : s.public_cards[0];

        std::vector<Action> round1_betting_actions;
        std::vector<Action> round2_betting_actions;
        int chance_events_seen = 0;
        for (Action entry : s.history) {
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
        keys_by_group[{player, own_card, public_card, round1_betting_actions, round2_betting_actions}]
            .insert(game.infoset_key(s));
    });

    for (auto& [group, keys] : keys_by_group) {
        REQUIRE(keys.size() == 1);
    }
}

TEST_CASE("leduc: legal_actions non-empty at every non-terminal, non-chance state") {  // V3
    LeducGame game;
    walk(game, game.initial_state(), [&](const State& s) {
        if (game.is_terminal(s) || game.is_chance(s)) return;
        REQUIRE_FALSE(game.legal_actions(s).empty());
    });
}

TEST_CASE("leduc: chance outcome probabilities sum to 1") {  // V6
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

TEST_CASE("leduc: raise cap enforced at 2 per round") {
    LeducGame game;
    State state = game.initial_state();
    state = game.apply_action(state, kChanceCardOffset + 0);  // deal card 0 to player 0
    state = game.apply_action(state, kChanceCardOffset + 2);  // deal card 2 to player 1
    state = game.apply_action(state, kActionCallCheck);       // player 0 checks
    state = game.apply_action(state, kActionRaise);           // player 1 raises (1st raise)
    state = game.apply_action(state, kActionRaise);           // player 0 raises (2nd raise, cap hit)

    REQUIRE_FALSE(game.is_terminal(state));
    std::vector<Action> legal = game.legal_actions(state);
    REQUIRE(std::find(legal.begin(), legal.end(), kActionRaise) == legal.end());
    REQUIRE(std::find(legal.begin(), legal.end(), kActionFold) != legal.end());
    REQUIRE(std::find(legal.begin(), legal.end(), kActionCallCheck) != legal.end());
}

TEST_CASE("leduc: split pot pays 0 to both players") {
    LeducGame game;
    State state = game.initial_state();
    // player 0 gets card 2 (Q, suit 0), player 1 gets card 3 (Q, suit 1) —
    // same rank, different suit; public card is card 4 (K, suit 0), matching
    // neither private card's rank, so the showdown is a genuine tie.
    state = game.apply_action(state, kChanceCardOffset + 2);
    state = game.apply_action(state, kChanceCardOffset + 3);
    state = game.apply_action(state, kActionCallCheck);  // round 1: player 0 checks
    state = game.apply_action(state, kActionCallCheck);  // round 1: player 1 checks (round closes)
    REQUIRE(game.is_chance(state));
    state = game.apply_action(state, kChanceCardOffset + 4);  // public card: K
    state = game.apply_action(state, kActionCallCheck);       // round 2: player 0 checks
    state = game.apply_action(state, kActionCallCheck);       // round 2: player 1 checks (terminal)

    REQUIRE(game.is_terminal(state));
    REQUIRE(game.terminal_utility(state, 0) == 0.0);
    REQUIRE(game.terminal_utility(state, 1) == 0.0);
}

TEST_CASE("leduc: round 2 bet doubles round 1's size") {
    LeducGame game;
    State state = game.initial_state();
    // player 0 gets card 4 (K), player 1 gets card 0 (J) — player 0 wins any
    // showdown outright, no pairing involved, so the payout is exactly the
    // round 2 bet size beyond antes.
    state = game.apply_action(state, kChanceCardOffset + 4);
    state = game.apply_action(state, kChanceCardOffset + 0);
    state = game.apply_action(state, kActionCallCheck);  // round 1: player 0 checks
    state = game.apply_action(state, kActionCallCheck);  // round 1: player 1 checks (round closes)
    state = game.apply_action(state, kChanceCardOffset + 2);  // public card: Q (pairs neither)
    state = game.apply_action(state, kActionRaise);           // round 2: player 0 raises (bet 4)
    state = game.apply_action(state, kActionCallCheck);       // round 2: player 1 calls (terminal)

    REQUIRE(game.is_terminal(state));
    // Each player ante'd 1; round 2's raise+call moves kRound2Bet (4) each.
    // Winner's net = pot - own contribution = (1+1+4+4) - (1+4) = 5.
    REQUIRE(game.terminal_utility(state, 0) == 5.0);
    REQUIRE(game.terminal_utility(state, 1) == -5.0);
}
