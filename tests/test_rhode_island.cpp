#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include <algorithm>
#include <cstdint>
#include <unordered_map>
#include <utility>
#include <vector>

#include "game/card.hpp"
#include "game/game.hpp"
#include "game/rhode_island.hpp"
#include "tree_walk.hpp"

using cfr::make_card;
using namespace cfr::game;

static_assert(GameLike<RhodeIslandGame>);

namespace {

struct BettingRoundTopology {
    long long node_count;
    long long closing_leaf_count;
};

BettingRoundTopology enumerate_betting_round_topology(int raises_used, bool facing_wager, bool is_round_start,
                                                        int max_raises_per_round) {
    long long node_count = 1;
    long long closing_leaf_count = 0;

    if (!facing_wager) {
        if (is_round_start) {
            BettingRoundTopology check_subtree =
                enumerate_betting_round_topology(raises_used, false, false, max_raises_per_round);
            node_count += check_subtree.node_count;
            closing_leaf_count += check_subtree.closing_leaf_count;
        } else {
            node_count += 1;
            closing_leaf_count += 1;
        }
        BettingRoundTopology bet_subtree =
            enumerate_betting_round_topology(raises_used + 1, true, false, max_raises_per_round);
        node_count += bet_subtree.node_count;
        closing_leaf_count += bet_subtree.closing_leaf_count;
    } else {
        node_count += 1;
        node_count += 1;
        closing_leaf_count += 1;
        if (raises_used < max_raises_per_round) {
            BettingRoundTopology raise_subtree =
                enumerate_betting_round_topology(raises_used + 1, true, false, max_raises_per_round);
            node_count += raise_subtree.node_count;
            closing_leaf_count += raise_subtree.closing_leaf_count;
        }
    }
    return {node_count, closing_leaf_count};
}

long long composed_three_round_betting_node_count(int max_raises_per_round) {
    BettingRoundTopology round1_topology = enumerate_betting_round_topology(0, false, true, max_raises_per_round);
    BettingRoundTopology round2_topology = enumerate_betting_round_topology(0, false, true, max_raises_per_round);
    BettingRoundTopology round3_topology = enumerate_betting_round_topology(0, false, true, max_raises_per_round);
    return round1_topology.node_count +
           round1_topology.closing_leaf_count *
               (round2_topology.node_count + round2_topology.closing_leaf_count * round3_topology.node_count);
}

constexpr long long kRihOrderedHoleAssignmentCount = 52 * 51;
constexpr long long kRihFlopOutcomeCount = 50;
constexpr long long kRihTurnOutcomeCount = 49;

long long full_game_node_count(int max_raises_per_round) {
    BettingRoundTopology round_topology = enumerate_betting_round_topology(0, false, true, max_raises_per_round);
    long long nodes_per_hole_assignment =
        round_topology.node_count +
        round_topology.closing_leaf_count * kRihFlopOutcomeCount *
            (round_topology.node_count +
             round_topology.closing_leaf_count * kRihTurnOutcomeCount * round_topology.node_count);
    return kRihOrderedHoleAssignmentCount * nodes_per_hole_assignment;
}

long long count_fixed_card_engine_nodes(const RhodeIslandGame& game, const State& state, Action fixed_flop_action,
                                         Action fixed_turn_action) {
    long long total = 1;
    if (game.is_terminal(state)) return total;
    if (game.is_chance(state)) {
        Action next_card_action = state.public_count == 0 ? fixed_flop_action : fixed_turn_action;
        total +=
            count_fixed_card_engine_nodes(game, game.apply_action(state, next_card_action), fixed_flop_action,
                                           fixed_turn_action);
        return total;
    }
    for (Action action : game.legal_actions(state)) {
        total += count_fixed_card_engine_nodes(game, game.apply_action(state, action), fixed_flop_action,
                                                 fixed_turn_action);
    }
    return total;
}

}

TEST_CASE("RIH betting-subtree node count matches combinatorial formula") {
    RhodeIslandGame game;
    State state = game.initial_state();
    state = game.apply_action(state, kChanceCardOffset + make_card(0, 0));
    state = game.apply_action(state, kChanceCardOffset + make_card(1, 0));

    Action fixed_flop_action = kChanceCardOffset + make_card(2, 0);
    Action fixed_turn_action = kChanceCardOffset + make_card(3, 0);

    long long engine_node_count = count_fixed_card_engine_nodes(game, state, fixed_flop_action, fixed_turn_action);
    long long formula_node_count = composed_three_round_betting_node_count(kRihMaxRaisesPerRound);

    REQUIRE(engine_node_count == formula_node_count);
}

TEST_CASE("RIH per-round betting topology matches the hand enumeration") {
    BettingRoundTopology round_topology = enumerate_betting_round_topology(0, false, true, kRihMaxRaisesPerRound);
    REQUIRE(round_topology.node_count == 21);
    REQUIRE(round_topology.closing_leaf_count == 7);
}

TEST_CASE("RIH full-game node count exceeds the published 3.1e9 figure") {
    long long betting_and_board_nodes = full_game_node_count(kRihMaxRaisesPerRound);
    long long hole_deal_chance_node_count = 1 + kRihDeckSize;
    long long full_tree_node_count = hole_deal_chance_node_count + betting_and_board_nodes;

    REQUIRE(betting_and_board_nodes == 6705372492LL);
    REQUIRE(full_tree_node_count == 6705372545LL);
    REQUIRE(full_tree_node_count > 3100000000LL);
}

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
    state = game.apply_action(state, kChanceCardOffset + 0);
    state = game.apply_action(state, kChanceCardOffset + 4);

    REQUIRE_FALSE(game.is_chance(state));
    REQUIRE(game.current_player(state) == 0);
    std::vector<Action> legal = game.legal_actions(state);
    REQUIRE(legal.size() == 2);
    REQUIRE(std::find(legal.begin(), legal.end(), kActionCallCheck) != legal.end());
    REQUIRE(std::find(legal.begin(), legal.end(), kActionRaise) != legal.end());
}

TEST_CASE("rhode island: raise cap enforced at 3 per round") {
    RhodeIslandGame game;
    State state = game.initial_state();
    state = game.apply_action(state, kChanceCardOffset + 0);
    state = game.apply_action(state, kChanceCardOffset + 4);
    state = game.apply_action(state, kActionCallCheck);
    state = game.apply_action(state, kActionRaise);
    state = game.apply_action(state, kActionRaise);
    state = game.apply_action(state, kActionRaise);

    REQUIRE_FALSE(game.is_terminal(state));
    std::vector<Action> legal = game.legal_actions(state);
    REQUIRE(std::find(legal.begin(), legal.end(), kActionRaise) == legal.end());
    REQUIRE(std::find(legal.begin(), legal.end(), kActionFold) != legal.end());
    REQUIRE(std::find(legal.begin(), legal.end(), kActionCallCheck) != legal.end());
}

TEST_CASE("rhode island: round transitions through flop and turn to terminal") {
    RhodeIslandGame game;
    State state = game.initial_state();
    state = game.apply_action(state, kChanceCardOffset + 0);
    state = game.apply_action(state, kChanceCardOffset + 4);

    state = game.apply_action(state, kActionCallCheck);
    state = game.apply_action(state, kActionCallCheck);
    REQUIRE(game.is_chance(state));

    std::vector<std::pair<Action, double>> flop_outcomes = game.chance_outcomes(state);
    state = game.apply_action(state, flop_outcomes.front().first);
    REQUIRE_FALSE(game.is_chance(state));
    REQUIRE_FALSE(game.is_terminal(state));
    REQUIRE(game.current_player(state) == 0);

    state = game.apply_action(state, kActionCallCheck);
    state = game.apply_action(state, kActionCallCheck);
    REQUIRE(game.is_chance(state));

    std::vector<std::pair<Action, double>> turn_outcomes = game.chance_outcomes(state);
    state = game.apply_action(state, turn_outcomes.front().first);
    REQUIRE_FALSE(game.is_chance(state));
    REQUIRE_FALSE(game.is_terminal(state));
    REQUIRE(game.current_player(state) == 0);

    state = game.apply_action(state, kActionCallCheck);
    state = game.apply_action(state, kActionCallCheck);
    REQUIRE(game.is_terminal(state));
    REQUIRE(rih_contributions(state)[0] + rih_contributions(state)[1] == 10);
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
    state = game.apply_action(state, kActionCallCheck);
    state = game.apply_action(state, kActionCallCheck);
    REQUIRE(game.is_chance(state));

    std::vector<std::pair<Action, double>> flop_outcomes = game.chance_outcomes(state);
    REQUIRE(flop_outcomes.size() == 50);
    double flop_probability_sum = 0.0;
    for (auto& [action, probability] : flop_outcomes) flop_probability_sum += probability;
    REQUIRE_THAT(flop_probability_sum, Catch::Matchers::WithinAbs(1.0, 1e-12));

    Action flop_dealt_action = flop_outcomes.front().first;
    state = game.apply_action(state, flop_dealt_action);
    state = game.apply_action(state, kActionCallCheck);
    state = game.apply_action(state, kActionCallCheck);
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
    state = game.apply_action(state, kChanceCardOffset + 0);
    state = game.apply_action(state, kChanceCardOffset + 4);

    state = game.apply_action(state, kActionCallCheck);
    state = game.apply_action(state, kActionRaise);
    state = game.apply_action(state, kActionFold);

    REQUIRE(game.is_terminal(state));
    REQUIRE(rih_contributions(state)[0] + rih_contributions(state)[1] == 20);
    REQUIRE(game.terminal_utility(state, 0) == -5.0);
    REQUIRE(game.terminal_utility(state, 1) == 5.0);
}

TEST_CASE("rhode island showdown: straight beats flush at showdown, an RIH-specific inversion") {
    RhodeIslandGame game;
    State state = game.initial_state();
    state = game.apply_action(state, kChanceCardOffset + make_card(8, 2));
    state = game.apply_action(state, kChanceCardOffset + make_card(3, 0));

    state = game.apply_action(state, kActionCallCheck);
    state = game.apply_action(state, kActionCallCheck);
    state = game.apply_action(state, kChanceCardOffset + make_card(4, 2));

    state = game.apply_action(state, kActionCallCheck);
    state = game.apply_action(state, kActionCallCheck);
    state = game.apply_action(state, kChanceCardOffset + make_card(5, 2));

    state = game.apply_action(state, kActionCallCheck);
    state = game.apply_action(state, kActionCallCheck);

    REQUIRE(game.is_terminal(state));
    REQUIRE(rih_contributions(state)[0] + rih_contributions(state)[1] == 10);
    REQUIRE(game.terminal_utility(state, 1) == 5.0);
    REQUIRE(game.terminal_utility(state, 0) == -5.0);
    REQUIRE(game.terminal_utility(state, 0) + game.terminal_utility(state, 1) == 0.0);
}

TEST_CASE("rhode island showdown: equal-ranked hands split the pot") {
    RhodeIslandGame game;
    State state = game.initial_state();
    state = game.apply_action(state, kChanceCardOffset + make_card(4, 2));
    state = game.apply_action(state, kChanceCardOffset + make_card(4, 3));

    state = game.apply_action(state, kActionCallCheck);
    state = game.apply_action(state, kActionCallCheck);
    state = game.apply_action(state, kChanceCardOffset + make_card(1, 0));

    state = game.apply_action(state, kActionCallCheck);
    state = game.apply_action(state, kActionCallCheck);
    state = game.apply_action(state, kChanceCardOffset + make_card(8, 1));

    state = game.apply_action(state, kActionCallCheck);
    state = game.apply_action(state, kActionCallCheck);

    REQUIRE(game.is_terminal(state));
    REQUIRE(game.terminal_utility(state, 0) == 0.0);
    REQUIRE(game.terminal_utility(state, 1) == 0.0);
}

TEST_CASE("rhode island V1: zero-sum at every terminal of the fixed-hole-cards subtree") {
    RhodeIslandGame game;
    State state = game.initial_state();
    state = game.apply_action(state, kChanceCardOffset + make_card(0, 0));
    state = game.apply_action(state, kChanceCardOffset + make_card(1, 0));

    long long terminals_visited = 0;
    walk(game, state, [&](const State& s) {
        if (!game.is_terminal(s)) return;
        REQUIRE(game.terminal_utility(s, 0) + game.terminal_utility(s, 1) == 0.0);
        ++terminals_visited;
    });
    REQUIRE(terminals_visited > 0);
}

TEST_CASE("rhode island V3: legal_actions non-empty at every non-terminal, non-chance state of the fixed-hole-cards "
          "subtree") {
    RhodeIslandGame game;
    State state = game.initial_state();
    state = game.apply_action(state, kChanceCardOffset + make_card(0, 0));
    state = game.apply_action(state, kChanceCardOffset + make_card(1, 0));

    walk(game, state, [&](const State& s) {
        if (game.is_terminal(s) || game.is_chance(s)) return;
        REQUIRE_FALSE(game.legal_actions(s).empty());
    });
}

namespace {

State permute_suits(const State& state, const std::array<int, 4>& suit_permutation) {
    auto remap = [&suit_permutation](int card) {
        return static_cast<int>(make_card(cfr::card_rank(static_cast<cfr::Card>(card)),
                                           suit_permutation[static_cast<std::size_t>(
                                               cfr::card_suit(static_cast<cfr::Card>(card)))]));
    };

    State permuted = state;
    for (std::size_t i = 0; i < 2; ++i) {
        if (state.private_cards[i] >= 0) {
            permuted.private_cards[i] = static_cast<std::int8_t>(remap(state.private_cards[i]));
        }
    }
    for (std::uint8_t i = 0; i < state.public_count; ++i) {
        permuted.public_cards[i] = static_cast<std::int8_t>(remap(state.public_cards[i]));
    }
    for (std::uint8_t i = 0; i < state.history_len; ++i) {
        if (state.history[i] >= kChanceCardOffset) {
            permuted.history[i] =
                static_cast<std::uint8_t>(kChanceCardOffset + remap(state.history[i] - kChanceCardOffset));
        }
    }
    return permuted;
}

}

TEST_CASE("rhode island: relabelling the suits changes neither payoffs nor the infoset index") {
    RhodeIslandGame game;
    State state = game.initial_state();
    state = game.apply_action(state, kChanceCardOffset + make_card(0, 0));
    state = game.apply_action(state, kChanceCardOffset + make_card(1, 1));

    const std::array<int, 4> rotate_suits = {1, 2, 3, 0};
    long long states_checked = 0;

    walk(game, state, [&](const State& s) {
        State permuted = permute_suits(s, rotate_suits);
        if (game.is_terminal(s)) {
            REQUIRE(game.terminal_utility(s, 0) == game.terminal_utility(permuted, 0));
            ++states_checked;
            return;
        }
        if (game.is_chance(s)) return;
        REQUIRE(game.infoset_index(s) < game.infoset_count());
        REQUIRE(game.infoset_index(s) == game.infoset_index(permuted));
        ++states_checked;
    });
    REQUIRE(states_checked > 0);
}

TEST_CASE("rhode island: infoset_label determines the infoset index") {
    RhodeIslandGame game;
    State state = game.initial_state();
    state = game.apply_action(state, kChanceCardOffset + make_card(0, 0));
    state = game.apply_action(state, kChanceCardOffset + make_card(1, 0));

    std::unordered_map<InfoSetKey, std::uint32_t> label_to_index;
    long long infosets_visited = 0;
    walk(game, state, [&](const State& s) {
        if (game.is_terminal(s) || game.is_chance(s)) return;
        auto [entry, inserted] = label_to_index.emplace(game.infoset_label(s), game.infoset_index(s));
        if (!inserted) REQUIRE(entry->second == game.infoset_index(s));
        ++infosets_visited;
    });
    REQUIRE(infosets_visited > 0);
}

TEST_CASE("rhode island V2: infoset_label and infoset_index hide the opponent's hole card") {
    RhodeIslandGame game;

    State preflop_state = game.initial_state();
    preflop_state = game.apply_action(preflop_state, kChanceCardOffset + make_card(0, 0));
    preflop_state = game.apply_action(preflop_state, kChanceCardOffset + make_card(1, 0));

    State preflop_state_other_opponent_hole = game.initial_state();
    preflop_state_other_opponent_hole =
        game.apply_action(preflop_state_other_opponent_hole, kChanceCardOffset + make_card(0, 0));
    preflop_state_other_opponent_hole =
        game.apply_action(preflop_state_other_opponent_hole, kChanceCardOffset + make_card(9, 3));

    REQUIRE(game.infoset_label(preflop_state) == game.infoset_label(preflop_state_other_opponent_hole));
    REQUIRE(game.infoset_index(preflop_state) == game.infoset_index(preflop_state_other_opponent_hole));

    State postflop_state = preflop_state;
    postflop_state = game.apply_action(postflop_state, kActionCallCheck);
    postflop_state = game.apply_action(postflop_state, kActionCallCheck);
    postflop_state = game.apply_action(postflop_state, kChanceCardOffset + make_card(5, 1));

    State postflop_state_other_opponent_hole = preflop_state_other_opponent_hole;
    postflop_state_other_opponent_hole = game.apply_action(postflop_state_other_opponent_hole, kActionCallCheck);
    postflop_state_other_opponent_hole = game.apply_action(postflop_state_other_opponent_hole, kActionCallCheck);
    postflop_state_other_opponent_hole =
        game.apply_action(postflop_state_other_opponent_hole, kChanceCardOffset + make_card(5, 1));

    REQUIRE(game.infoset_label(postflop_state) == game.infoset_label(postflop_state_other_opponent_hole));
    REQUIRE(game.infoset_index(postflop_state) == game.infoset_index(postflop_state_other_opponent_hole));
}

TEST_CASE("V26: rhode island re-raise line pot is exact at every step") {
    RhodeIslandGame game;
    auto pot_of = [](const State& s) {
        std::array<int, 2> contribution = rih_contributions(s);
        return contribution[0] + contribution[1];
    };
    State state = game.initial_state();
    state = game.apply_action(state, kChanceCardOffset + make_card(0, 0));
    state = game.apply_action(state, kChanceCardOffset + make_card(8, 1));
    REQUIRE(pot_of(state) == 10);
    state = game.apply_action(state, kActionCallCheck);
    REQUIRE(pot_of(state) == 10);
    state = game.apply_action(state, kActionRaise);
    REQUIRE(pot_of(state) == 20);
    state = game.apply_action(state, kActionRaise);
    REQUIRE(pot_of(state) == 40);
    state = game.apply_action(state, kActionCallCheck);
    REQUIRE(pot_of(state) == 50);
}

TEST_CASE("V26: rhode island showdown pays exactly half the pot across the fixed-hole subtree") {
    RhodeIslandGame game;
    State state = game.initial_state();
    state = game.apply_action(state, kChanceCardOffset + make_card(0, 0));
    state = game.apply_action(state, kChanceCardOffset + make_card(1, 0));

    long long showdown_terminals = 0;
    walk(game, state, [&](const State& terminal_candidate) {
        if (!game.is_terminal(terminal_candidate)) return;
        if (terminal_candidate.history[terminal_candidate.history_len - 1] == kActionFold) return;
        ++showdown_terminals;
        std::array<int, 2> contribution = rih_contributions(terminal_candidate);
        REQUIRE(contribution[0] == contribution[1]);
        int pot = contribution[0] + contribution[1];
        REQUIRE(pot % 2 == 0);
        double first_player_utility = game.terminal_utility(terminal_candidate, 0);
        double half_pot = pot / 2.0;
        REQUIRE((first_player_utility == 0.0 || first_player_utility == half_pot ||
                 first_player_utility == -half_pot));
    });
    REQUIRE(showdown_terminals > 0);
}
