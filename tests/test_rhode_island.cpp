// Rhode Island rules: node counts against combinatorial formulas,
// payoffs, suit symmetry and hidden information.

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <unordered_map>
#include <utility>
#include <vector>

#include "abstraction/rih_gameshrink.hpp"
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

BettingRoundTopology enumerate_betting_round_topology(
    int raises_used, bool facing_bet, bool is_round_start,
    int max_raises_per_round) {
    long long node_count = 1;
    long long closing_leaf_count = 0;

    if (!facing_bet) {
        if (is_round_start) {
            BettingRoundTopology check_subtree =
                enumerate_betting_round_topology(raises_used, false, false,
                                                 max_raises_per_round);
            node_count += check_subtree.node_count;
            closing_leaf_count += check_subtree.closing_leaf_count;
        } else {
            node_count += 1;
            closing_leaf_count += 1;
        }
        BettingRoundTopology bet_subtree = enumerate_betting_round_topology(
            raises_used + 1, true, false, max_raises_per_round);
        node_count += bet_subtree.node_count;
        closing_leaf_count += bet_subtree.closing_leaf_count;
    } else {
        node_count += 1;
        node_count += 1;
        closing_leaf_count += 1;
        if (raises_used < max_raises_per_round) {
            BettingRoundTopology raise_subtree =
                enumerate_betting_round_topology(raises_used + 1, true, false,
                                                 max_raises_per_round);
            node_count += raise_subtree.node_count;
            closing_leaf_count += raise_subtree.closing_leaf_count;
        }
    }
    return {node_count, closing_leaf_count};
}

long long composed_three_round_betting_node_count(int max_raises_per_round) {
    BettingRoundTopology round1_topology =
        enumerate_betting_round_topology(0, false, true, max_raises_per_round);
    BettingRoundTopology round2_topology =
        enumerate_betting_round_topology(0, false, true, max_raises_per_round);
    BettingRoundTopology round3_topology =
        enumerate_betting_round_topology(0, false, true, max_raises_per_round);
    return round1_topology.node_count +
           round1_topology.closing_leaf_count *
               (round2_topology.node_count +
                round2_topology.closing_leaf_count *
                    round3_topology.node_count);
}

constexpr long long kRihOrderedHoleAssignmentCount = 52 * 51;
constexpr long long kRihFlopOutcomeCount = 50;
constexpr long long kRihTurnOutcomeCount = 49;

long long full_game_node_count(int max_raises_per_round) {
    BettingRoundTopology round_topology =
        enumerate_betting_round_topology(0, false, true, max_raises_per_round);
    long long nodes_per_hole_assignment =
        round_topology.node_count +
        round_topology.closing_leaf_count * kRihFlopOutcomeCount *
            (round_topology.node_count + round_topology.closing_leaf_count *
                                             kRihTurnOutcomeCount *
                                             round_topology.node_count);
    return kRihOrderedHoleAssignmentCount * nodes_per_hole_assignment;
}

long long count_fixed_card_engine_nodes(const RhodeIslandGame& game,
                                        const State& state,
                                        Action fixed_flop_action,
                                        Action fixed_turn_action) {
    long long total = 1;
    if (game.is_terminal(state)) return total;
    if (game.is_chance(state)) {
        Action next_card_action =
            state.board_count == 0 ? fixed_flop_action : fixed_turn_action;
        total += count_fixed_card_engine_nodes(
            game, game.apply_action(state, next_card_action), fixed_flop_action,
            fixed_turn_action);
        return total;
    }
    for (Action action : game.legal_actions(state)) {
        total += count_fixed_card_engine_nodes(
            game, game.apply_action(state, action), fixed_flop_action,
            fixed_turn_action);
    }
    return total;
}

struct SequenceFormSize {
    std::array<long long, 2> infosets, sequences;
};

SequenceFormSize count_fixed_card_sequence_form_size(
    const RhodeIslandGame& game, const State& state, Action fixed_flop_action,
    Action fixed_turn_action, const std::array<long long, 3>& classes) {
    SequenceFormSize totals{{0, 0}, {1, 1}};
    auto visit = [&](auto&& self, const State& node) -> void {
        if (game.is_terminal(node)) return;
        if (game.is_chance(node)) {
            Action next_card_action =
                node.board_count == 0 ? fixed_flop_action : fixed_turn_action;
            self(self, game.apply_action(node, next_card_action));
            return;
        }
        Player player = game.current_player(node);
        long long own_information_count = classes[node.board_count];
        auto legal = game.legal_actions(node);
        totals.infosets[player] += own_information_count;
        totals.sequences[player] +=
            own_information_count * static_cast<long long>(legal.size());
        for (Action action : legal) self(self, game.apply_action(node, action));
    };
    visit(visit, state);
    return totals;
}

// Sequence-form size with classes[r] own-information values in round
// r. Betting does not depend on the cards, so any fixed deal will do.
SequenceFormSize rih_sequence_form_size(
    const std::array<long long, 3>& classes) {
    RhodeIslandGame game;
    State state = game.initial_state();
    state = game.apply_action(state, kChanceCardOffset + make_card(0, 0));
    state = game.apply_action(state, kChanceCardOffset + make_card(1, 0));
    return count_fixed_card_sequence_form_size(
        game, state, kChanceCardOffset + make_card(2, 0),
        kChanceCardOffset + make_card(3, 0), classes);
}

}

TEST_CASE("RIH betting-subtree node count matches combinatorial formula") {
    RhodeIslandGame game;
    State state = game.initial_state();
    state = game.apply_action(state, kChanceCardOffset + make_card(0, 0));
    state = game.apply_action(state, kChanceCardOffset + make_card(1, 0));

    Action fixed_flop_action = kChanceCardOffset + make_card(2, 0);
    Action fixed_turn_action = kChanceCardOffset + make_card(3, 0);

    long long engine_node_count = count_fixed_card_engine_nodes(
        game, state, fixed_flop_action, fixed_turn_action);
    long long formula_node_count =
        composed_three_round_betting_node_count(kRihMaxRaisesPerRound);

    REQUIRE(engine_node_count == formula_node_count);
}

TEST_CASE("RIH per-round betting topology matches the hand enumeration") {
    BettingRoundTopology round_topology =
        enumerate_betting_round_topology(0, false, true, kRihMaxRaisesPerRound);
    REQUIRE(round_topology.node_count == 21);
    REQUIRE(round_topology.closing_leaf_count == 7);
}

TEST_CASE("RIH full-game node count exceeds the published 3.1e9 figure") {
    long long betting_and_board_nodes =
        full_game_node_count(kRihMaxRaisesPerRound);
    long long hole_deal_chance_node_count = 1 + cfr::kCardCount;
    long long full_tree_node_count =
        hole_deal_chance_node_count + betting_and_board_nodes;

    REQUIRE(betting_and_board_nodes == 6705372492LL);
    REQUIRE(full_tree_node_count == 6705372545LL);
    REQUIRE(full_tree_node_count > 3100000000LL);
}

TEST_CASE("RIH unabstracted sequence-form size matches the paper") {
    // Gilpin & Sandholm, J. ACM 2007, p. 25:4.
    SequenceFormSize size = rih_sequence_form_size({52, 52 * 51, 52 * 51 * 50});
    long long rows = size.sequences[0] + size.infosets[1] + 1;
    long long columns = size.sequences[1] + size.infosets[0] + 1;
    for (std::size_t player = 0; player < 2; ++player) {
        REQUIRE(size.infosets[player] == 26064064LL);
        REQUIRE(size.sequences[player] == 65160161LL);
    }
    REQUIRE(rows == 91224226LL);
    REQUIRE(columns == 91224226LL);
}

TEST_CASE("RIH GameShrink sequence-form size matches the paper") {
    // Gilpin & Sandholm, J. ACM 2007, p. 25:4: 1,237,238 rows and
    // columns after GameShrink.
    cfr::abstraction::RihPartition partition =
        cfr::abstraction::RihPartition::compute();
    SequenceFormSize size = rih_sequence_form_size({partition.class_count(0),
                                                    partition.class_count(1),
                                                    partition.class_count(2)});
    REQUIRE(size.sequences[0] + size.infosets[1] + 1 == 1237238LL);
    REQUIRE(size.sequences[1] + size.infosets[0] + 1 == 1237238LL);
}

TEST_CASE("rhode island satisfies GameLike", "[concept]") {
    REQUIRE(GameLike<RhodeIslandGame>);
}

TEST_CASE("rhode island: initial state is a chance node") {
    RhodeIslandGame game;
    State state = game.initial_state();
    REQUIRE(game.is_chance(state));
}

TEST_CASE(
    "rhode island: with both hole cards dealt, player 0 may check or raise") {
    RhodeIslandGame game;
    State state = game.initial_state();
    state = game.apply_action(state, kChanceCardOffset + 0);
    state = game.apply_action(state, kChanceCardOffset + 4);

    REQUIRE_FALSE(game.is_chance(state));
    REQUIRE(game.current_player(state) == 0);
    auto legal = game.legal_actions(state);
    REQUIRE(legal.size() == 2);
    REQUIRE(std::find(legal.begin(), legal.end(), kActionCallCheck) !=
            legal.end());
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
    auto legal = game.legal_actions(state);
    REQUIRE(std::find(legal.begin(), legal.end(), kActionRaise) == legal.end());
    REQUIRE(std::find(legal.begin(), legal.end(), kActionFold) != legal.end());
    REQUIRE(std::find(legal.begin(), legal.end(), kActionCallCheck) !=
            legal.end());
}

TEST_CASE("rhode island: round transitions through flop and turn to terminal") {
    RhodeIslandGame game;
    State state = game.initial_state();
    state = game.apply_action(state, kChanceCardOffset + 0);
    state = game.apply_action(state, kChanceCardOffset + 4);

    state = game.apply_action(state, kActionCallCheck);
    state = game.apply_action(state, kActionCallCheck);
    REQUIRE(game.is_chance(state));

    auto flop_outcomes = game.chance_outcomes(state);
    state = game.apply_action(state, flop_outcomes[0].action);
    REQUIRE_FALSE(game.is_chance(state));
    REQUIRE_FALSE(game.is_terminal(state));
    REQUIRE(game.current_player(state) == 0);

    state = game.apply_action(state, kActionCallCheck);
    state = game.apply_action(state, kActionCallCheck);
    REQUIRE(game.is_chance(state));

    auto turn_outcomes = game.chance_outcomes(state);
    state = game.apply_action(state, turn_outcomes[0].action);
    REQUIRE_FALSE(game.is_chance(state));
    REQUIRE_FALSE(game.is_terminal(state));
    REQUIRE(game.current_player(state) == 0);

    state = game.apply_action(state, kActionCallCheck);
    state = game.apply_action(state, kActionCallCheck);
    REQUIRE(game.is_terminal(state));
    REQUIRE(state.contribution[0] + state.contribution[1] == 10);
}

TEST_CASE(
    "rhode island: chance probabilities sum to 1 and exclude dealt cards") {
    RhodeIslandGame game;
    State state = game.initial_state();
    REQUIRE(game.is_chance(state));

    auto hole0_outcomes = game.chance_outcomes(state);
    REQUIRE(hole0_outcomes.size() == 52);
    double hole0_probability_sum = 0.0;
    for (auto& [action, probability] : hole0_outcomes)
        hole0_probability_sum += probability;
    REQUIRE_THAT(hole0_probability_sum, Catch::Matchers::WithinAbs(1.0, 1e-12));

    Action hole0_dealt_action = hole0_outcomes[0].action;
    state = game.apply_action(state, hole0_dealt_action);

    auto hole1_outcomes = game.chance_outcomes(state);
    REQUIRE(hole1_outcomes.size() == 51);
    for (auto& [action, probability] : hole1_outcomes)
        REQUIRE(action != hole0_dealt_action);
    double hole1_probability_sum = 0.0;
    for (auto& [action, probability] : hole1_outcomes)
        hole1_probability_sum += probability;
    REQUIRE_THAT(hole1_probability_sum, Catch::Matchers::WithinAbs(1.0, 1e-12));

    state = game.apply_action(state, hole1_outcomes[0].action);
    state = game.apply_action(state, kActionCallCheck);
    state = game.apply_action(state, kActionCallCheck);
    REQUIRE(game.is_chance(state));

    auto flop_outcomes = game.chance_outcomes(state);
    REQUIRE(flop_outcomes.size() == 50);
    double flop_probability_sum = 0.0;
    for (auto& [action, probability] : flop_outcomes)
        flop_probability_sum += probability;
    REQUIRE_THAT(flop_probability_sum, Catch::Matchers::WithinAbs(1.0, 1e-12));

    Action flop_dealt_action = flop_outcomes[0].action;
    state = game.apply_action(state, flop_dealt_action);
    state = game.apply_action(state, kActionCallCheck);
    state = game.apply_action(state, kActionCallCheck);
    REQUIRE(game.is_chance(state));

    auto turn_outcomes = game.chance_outcomes(state);
    REQUIRE(turn_outcomes.size() == 49);
    for (auto& [action, probability] : turn_outcomes)
        REQUIRE(action != flop_dealt_action);
    double turn_probability_sum = 0.0;
    for (auto& [action, probability] : turn_outcomes)
        turn_probability_sum += probability;
    REQUIRE_THAT(turn_probability_sum, Catch::Matchers::WithinAbs(1.0, 1e-12));
}

// The fast draw must pick the same card as summing chance_outcomes,
// including the rolls that land on a partial sum.
void expect_same_draw(const RhodeIslandGame& game, const State& state) {
    auto outcomes = game.chance_outcomes(state);
    REQUIRE(outcomes.size() >= 1);
    double cumulative = 0.0;
    for (std::size_t i = 0; i + 1 < outcomes.size(); ++i) {
        cumulative += outcomes[i].probability;
        double below = std::nextafter(cumulative, 0.0);
        REQUIRE(game.sample_chance_action(state, below) == outcomes[i].action);
        REQUIRE(game.sample_chance_action(state, cumulative) !=
                outcomes[i].action);
    }
    REQUIRE(game.sample_chance_action(state, 0.0) == outcomes[0].action);
    REQUIRE(game.sample_chance_action(state, std::nextafter(1.0, 0.0)) ==
            outcomes[outcomes.size() - 1].action);
}

TEST_CASE("rhode island chance sample matches the cumulative draw") {
    RhodeIslandGame game;
    State state = game.initial_state();
    expect_same_draw(game, state);
    for (int held = 0; held < cfr::kCardCount; ++held) {
        State one =
            game.apply_action(game.initial_state(), kChanceCardOffset + held);
        expect_same_draw(game, one);
    }
    state = game.apply_action(state, kChanceCardOffset + 0);
    state = game.apply_action(state, kChanceCardOffset + 17);
    state = game.apply_action(state, kActionCallCheck);
    state = game.apply_action(state, kActionCallCheck);
    expect_same_draw(game, state);
    state = game.apply_action(state, kChanceCardOffset + 51);
    state = game.apply_action(state, kActionRaise);
    state = game.apply_action(state, kActionCallCheck);
    expect_same_draw(game, state);
}

TEST_CASE(
    "rhode island: a round 1 fold pays the raiser the folder's contribution") {
    RhodeIslandGame game;
    State state = game.initial_state();
    state = game.apply_action(state, kChanceCardOffset + 0);
    state = game.apply_action(state, kChanceCardOffset + 4);

    state = game.apply_action(state, kActionCallCheck);
    state = game.apply_action(state, kActionRaise);
    state = game.apply_action(state, kActionFold);

    REQUIRE(game.is_terminal(state));
    REQUIRE(state.contribution[0] + state.contribution[1] == 20);
    REQUIRE(game.terminal_utility(state, 0) == -5.0);
    REQUIRE(game.terminal_utility(state, 1) == 5.0);
}

TEST_CASE(
    "rhode island showdown: a straight beats a flush, unlike standard poker") {
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
    REQUIRE(state.contribution[0] + state.contribution[1] == 10);
    REQUIRE(game.terminal_utility(state, 1) == 5.0);
    REQUIRE(game.terminal_utility(state, 0) == -5.0);
    REQUIRE(game.terminal_utility(state, 0) + game.terminal_utility(state, 1) ==
            0.0);
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

TEST_CASE(
    "rhode island: zero-sum at every terminal of the fixed-hole subtree") {
    RhodeIslandGame game;
    State state = game.initial_state();
    state = game.apply_action(state, kChanceCardOffset + make_card(0, 0));
    state = game.apply_action(state, kChanceCardOffset + make_card(1, 0));

    long long terminals_visited = 0;
    walk(game, state, [&](const State& s) {
        if (!game.is_terminal(s)) return;
        REQUIRE(game.terminal_utility(s, 0) + game.terminal_utility(s, 1) ==
                0.0);
        ++terminals_visited;
    });
    REQUIRE(terminals_visited > 0);
}

TEST_CASE(
    "rhode island: legal_actions is non-empty across the fixed-hole subtree") {
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

State permute_suits(const State& state,
                    const std::array<int, 4>& suit_permutation) {
    auto remap = [&suit_permutation](int card) {
        return static_cast<int>(
            make_card(cfr::card_rank(static_cast<cfr::Card>(card)),
                      suit_permutation[static_cast<std::size_t>(
                          cfr::card_suit(static_cast<cfr::Card>(card)))]));
    };

    State permuted = state;
    for (std::size_t i = 0; i < 2; ++i) {
        if (state.hole_cards[i] >= 0) {
            permuted.hole_cards[i] =
                static_cast<std::int8_t>(remap(state.hole_cards[i]));
        }
    }
    std::size_t board_count =
        std::min<std::size_t>(state.board_count, kMaxBoardCards);
    for (std::size_t i = 0; i < board_count; ++i) {
        permuted.board_cards[i] =
            static_cast<std::int8_t>(remap(state.board_cards[i]));
    }
    for (std::uint8_t i = 0; i < state.history_len; ++i) {
        if (state.history[i] >= kChanceCardOffset) {
            permuted.history[i] = static_cast<std::uint8_t>(
                kChanceCardOffset +
                remap(state.history[i] - kChanceCardOffset));
        }
    }
    return permuted;
}

}

TEST_CASE(
    "rhode island: relabelling suits keeps payoffs and the infoset index") {
    RhodeIslandGame game;
    State state = game.initial_state();
    state = game.apply_action(state, kChanceCardOffset + make_card(0, 0));
    state = game.apply_action(state, kChanceCardOffset + make_card(1, 1));

    const std::array<int, 4> rotate_suits = {1, 2, 3, 0};
    long long states_checked = 0;

    walk(game, state, [&](const State& s) {
        State permuted = permute_suits(s, rotate_suits);
        if (game.is_terminal(s)) {
            REQUIRE(game.terminal_utility(s, 0) ==
                    game.terminal_utility(permuted, 0));
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

    std::unordered_map<InfosetLabel, std::uint32_t> label_to_index;
    long long infosets_visited = 0;
    walk(game, state, [&](const State& s) {
        if (game.is_terminal(s) || game.is_chance(s)) return;
        auto [entry, inserted] = label_to_index.emplace(game.infoset_label(s),
                                                        game.infoset_index(s));
        if (!inserted) REQUIRE(entry->second == game.infoset_index(s));
        ++infosets_visited;
    });
    REQUIRE(infosets_visited > 0);
}

TEST_CASE(
    "rhode island: infoset label and index hide the opponent's hole card") {
    RhodeIslandGame game;

    State preflop_state = game.initial_state();
    preflop_state =
        game.apply_action(preflop_state, kChanceCardOffset + make_card(0, 0));
    preflop_state =
        game.apply_action(preflop_state, kChanceCardOffset + make_card(1, 0));

    State preflop_state_other_opponent_hole = game.initial_state();
    preflop_state_other_opponent_hole = game.apply_action(
        preflop_state_other_opponent_hole, kChanceCardOffset + make_card(0, 0));
    preflop_state_other_opponent_hole = game.apply_action(
        preflop_state_other_opponent_hole, kChanceCardOffset + make_card(9, 3));

    REQUIRE(game.infoset_label(preflop_state) ==
            game.infoset_label(preflop_state_other_opponent_hole));
    REQUIRE(game.infoset_index(preflop_state) ==
            game.infoset_index(preflop_state_other_opponent_hole));

    State postflop_state = preflop_state;
    postflop_state = game.apply_action(postflop_state, kActionCallCheck);
    postflop_state = game.apply_action(postflop_state, kActionCallCheck);
    postflop_state =
        game.apply_action(postflop_state, kChanceCardOffset + make_card(5, 1));

    State postflop_state_other_opponent_hole =
        preflop_state_other_opponent_hole;
    postflop_state_other_opponent_hole =
        game.apply_action(postflop_state_other_opponent_hole, kActionCallCheck);
    postflop_state_other_opponent_hole =
        game.apply_action(postflop_state_other_opponent_hole, kActionCallCheck);
    postflop_state_other_opponent_hole =
        game.apply_action(postflop_state_other_opponent_hole,
                          kChanceCardOffset + make_card(5, 1));

    REQUIRE(game.infoset_label(postflop_state) ==
            game.infoset_label(postflop_state_other_opponent_hole));
    REQUIRE(game.infoset_index(postflop_state) ==
            game.infoset_index(postflop_state_other_opponent_hole));
}

TEST_CASE("rhode island re-raise line pot is exact at every step") {
    RhodeIslandGame game;
    auto pot_of = [](const State& s) {
        std::array<int, 2> contributions = s.contribution;
        return contributions[0] + contributions[1];
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

TEST_CASE(
    "rhode island showdown pays half the pot across the fixed-hole subtree") {
    RhodeIslandGame game;
    State state = game.initial_state();
    state = game.apply_action(state, kChanceCardOffset + make_card(0, 0));
    state = game.apply_action(state, kChanceCardOffset + make_card(1, 0));

    long long showdown_terminals = 0;
    walk(game, state, [&](const State& terminal_candidate) {
        if (!game.is_terminal(terminal_candidate)) return;
        if (terminal_candidate.history[terminal_candidate.history_len - 1] ==
            kActionFold)
            return;
        ++showdown_terminals;
        std::array<int, 2> contributions = terminal_candidate.contribution;
        REQUIRE(contributions[0] == contributions[1]);
        int pot = contributions[0] + contributions[1];
        REQUIRE(pot % 2 == 0);
        double first_player_utility =
            game.terminal_utility(terminal_candidate, 0);
        double half_pot = pot / 2.0;
        REQUIRE((first_player_utility == 0.0 ||
                 first_player_utility == half_pot ||
                 first_player_utility == -half_pot));
    });
    REQUIRE(showdown_terminals > 0);
}
namespace {
using Actions = std::vector<Action>;
State play(const RhodeIslandGame& game, State state, const Actions& line,
           int card = -1) {
    for (Action action : line) state = game.apply_action(state, action);
    if (card >= 0) state = game.apply_action(state, kChanceCardOffset + card);
    return state;
}
State deal_two(const RhodeIslandGame& game, int first, int second) {
    return play(game, play(game, game.initial_state(), {}, first), {}, second);
}
int spare(int first, int second = -1, int third = -1) {
    for (int card = 0; card < cfr::kCardCount; ++card)
        if (card != first && card != second && card != third) return card;
    return 0;
}
std::uint16_t cls(const cfr::abstraction::RihPartition& partition, int hole,
                  int flop = -1, int turn = -1) {
    std::array<cfr::Card, 3> cards{static_cast<cfr::Card>(hole)};
    std::size_t count = 1;
    if (flop >= 0) cards[count++] = static_cast<cfr::Card>(flop);
    if (turn >= 0) cards[count++] = static_cast<cfr::Card>(turn);
    return partition.class_of(std::span<const cfr::Card>(cards.data(), count));
}
std::pair<std::vector<Actions>, std::vector<Actions>> round_lines(
    const RhodeIslandGame& game) {
    std::vector<Actions> open, closed;
    Actions prefix;
    auto visit = [&](auto&& self, const State& node) -> void {
        if (game.is_terminal(node)) return;
        if (game.is_chance(node)) {
            closed.push_back(prefix);
            return;
        }
        open.push_back(prefix);
        for (Action action : game.legal_actions(node)) {
            prefix.push_back(action);
            self(self, game.apply_action(node, action));
            prefix.pop_back();
        }
    };
    visit(visit, deal_two(game, 0, 1));
    return {open, closed};
}
template <class Fn>
void cross(const RhodeIslandGame& game, const std::vector<Actions>& open,
           const std::vector<Actions>& closed, int hole, int flop, int turn,
           Fn fn) {
    int opponent = spare(hole, flop, turn);
    auto reach = [&](auto&& self, State s0, State s1, int round) -> void {
        if (round == (flop >= 0) + (turn >= 0)) {
            for (const Actions& line : open)
                fn(play(game, line.size() % 2 == 0 ? s0 : s1, line));
            return;
        }
        int card = round == 0 ? flop : turn;
        for (const Actions& close : closed)
            self(self, play(game, s0, close, card), play(game, s1, close, card),
                 round + 1);
    };
    reach(reach, deal_two(game, hole, opponent), deal_two(game, opponent, hole),
          0);
}
}
TEST_CASE("rhode island closing sequences are 0 through 6") {
    RhodeIslandGame game;
    auto lines = round_lines(game);
    std::vector<int> codes;
    for (const Actions& line : lines.second)
        codes.push_back(play(game, deal_two(game, 0, 1), line, 2).closing[0]);
    std::sort(codes.begin(), codes.end());
    REQUIRE(codes == (std::vector<int>{0, 1, 2, 3, 4, 5, 6}));
}
TEST_CASE("rhode island gameshrink index hits every row once") {
    RhodeIslandGame game;
    const auto partition = cfr::abstraction::RihPartition::compute().refined();
    REQUIRE(game.infoset_count() == 3142152);
    auto [open, closed] = round_lines(game);
    std::vector<int> rep0(partition.class_count(0), -1);
    std::vector<int> rep1(partition.class_count(1), -1);
    std::vector<int> rep2(partition.class_count(2), -1);
    for (int i = 0, deck = cfr::kCardCount; i < deck * deck * deck; ++i) {
        int hole = i / (deck * deck), flop = i / deck % deck, turn = i % deck;
        if (rep0[cls(partition, hole)] < 0) rep0[cls(partition, hole)] = hole;
        if (flop != hole && rep1[cls(partition, hole, flop)] < 0)
            rep1[cls(partition, hole, flop)] = hole * 64 + flop;
        if (flop != hole && turn != hole && turn != flop &&
            rep2[cls(partition, hole, flop, turn)] < 0)
            rep2[cls(partition, hole, flop, turn)] =
                (hole * 64 + flop) * 64 + turn;
    }
    std::vector<int> seen(game.infoset_count(), 0);
    int bad = 0;
    auto mark = [&](const State& state) {
        std::uint32_t index = game.infoset_index(state);
        if (index >= seen.size() || seen[index] != 0) ++bad;
        if (index < seen.size() && seen[index] == 0) seen[index] = 1;
    };
    for (int hole : rep0) cross(game, open, closed, hole, -1, -1, mark);
    for (int packed : rep1)
        cross(game, open, closed, packed / 64, packed % 64, -1, mark);
    for (int packed : rep2)
        cross(game, open, closed, packed / 4096, packed / 64 % 64, packed % 64,
              mark);
    REQUIRE(bad == 0);
    REQUIRE(std::count(seen.begin(), seen.end(), 1) ==
            std::ptrdiff_t(seen.size()));
}
TEST_CASE("rhode island infoset index matches player round betting and class") {
    RhodeIslandGame game;
    const auto partition = cfr::abstraction::RihPartition::compute().refined();
    auto [open, closed] = round_lines(game);
    std::unordered_map<std::string, std::uint32_t> by_key;
    std::unordered_map<std::uint32_t, std::string> by_index;
    int probes = 0, misses = 0;
    auto consider = [&](const State& state) {
        Player player = game.current_player(state);
        for (int hole = 0; hole < cfr::kCardCount; ++hole) {
            if ((state.board_count >= 1 && state.board_cards[0] == hole) ||
                (state.board_count >= 2 && state.board_cards[1] == hole))
                continue;
            State probe = state;
            game.set_hole_card(probe, player, hole);
            std::uint16_t id =
                cls(partition, hole,
                    state.board_count >= 1 ? state.board_cards[0] : -1,
                    state.board_count >= 2 ? state.board_cards[1] : -1);
            std::string key{static_cast<char>(player),
                            static_cast<char>(state.board_count),
                            static_cast<char>(id), static_cast<char>(id >> 8)};
            for (std::uint8_t i = 0; i < probe.history_len; ++i)
                if (probe.history[i] < kChanceCardOffset)
                    key.push_back(static_cast<char>(probe.history[i]));
            std::uint32_t index = game.infoset_index(probe);
            ++probes;
            auto [at_key, fresh] = by_key.emplace(key, index);
            misses += index >= game.infoset_count() ||
                      (!fresh && at_key->second != index);
            auto [at_index, fresh_index] = by_index.emplace(index, key);
            misses += index < game.infoset_count() && !fresh_index &&
                      at_index->second != key;
        }
    };
    // Preflop, then flop card 0, then turn card 1.
    for (int step = 0; step < 3; ++step)
        cross(game, open, closed, 2, step == 0 ? -1 : 0, step == 2 ? 1 : -1,
              consider);
    REQUIRE(probes > 20000);
    REQUIRE(misses == 0);
}
