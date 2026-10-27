// Rhode Island: published tree sizes, payoffs, suit symmetry, hidden cards.

#include <catch2/catch_test_macros.hpp>

#include <cmath>
#include <cstdint>
#include <unordered_map>
#include <utility>
#include <vector>

#include "abstraction/rih_gameshrink.hpp"
#include "game/card.hpp"
#include "game/game.hpp"
#include "game/rhode_island.hpp"
#include "oracles/rih_label.hpp"
#include "tree_walk.hpp"

using cfr::make_card;
using namespace cfr::game;

namespace {

struct SequenceFormSize {
    std::array<long long, 2> infosets, sequences;
};

// classes[r] is the number of own-information values in round r.
// Betting does not depend on the cards, so one fixed deal stands for
// every deal. That is what makes 91,224,226 pin the betting topology,
// including the three-raise cap.
SequenceFormSize rih_sequence_form_size(
    const std::array<long long, 3>& classes) {
    RhodeIslandGame game;
    State state = game.initial_state();
    state = game.apply_action(state, kChanceCardOffset + make_card(0, 0));
    state = game.apply_action(state, kChanceCardOffset + make_card(1, 0));
    Action fixed_flop = kChanceCardOffset + make_card(2, 0);
    Action fixed_turn = kChanceCardOffset + make_card(3, 0);
    SequenceFormSize totals{{0, 0}, {1, 1}};
    auto visit = [&](auto&& self, const State& node) -> void {
        if (game.is_terminal(node)) return;
        if (game.is_chance(node)) {
            Action next = node.board_count == 0 ? fixed_flop : fixed_turn;
            self(self, game.apply_action(node, next));
            return;
        }
        Player player = game.current_player(node);
        long long hands = classes[node.board_count];
        auto legal = game.legal_actions(node);
        totals.infosets[player] += hands;
        totals.sequences[player] +=
            hands * static_cast<long long>(legal.size());
        for (Action action : legal) self(self, game.apply_action(node, action));
    };
    visit(visit, state);
    return totals;
}

// One chance node: distinct undealt cards, probabilities summing to 1.
// Adds the number of longer deal prefixes to nodes; returns failures.
int note_chance(const RhodeIslandGame& game, const State& state,
                long long& nodes) {
    auto outcomes = game.chance_outcomes(state);
    double sum = 0.0;
    std::uint64_t seen = 0;
    std::uint64_t dealt = 0;
    int bad = 0;
    auto mark = [&](int card) {
        if (card >= 0) dealt |= std::uint64_t{1} << static_cast<unsigned>(card);
    };
    mark(state.hole_cards[0]);
    mark(state.hole_cards[1]);
    for (std::uint8_t i = 0; i < state.board_count; ++i)
        mark(state.board_cards[i]);
    for (auto [action, probability] : outcomes) {
        sum += probability;
        int card = action - kChanceCardOffset;
        if (card < 0 || card >= cfr::kCardCount) {
            ++bad;
            continue;
        }
        std::uint64_t bit = std::uint64_t{1} << static_cast<unsigned>(card);
        if ((seen & bit) != 0 || (dealt & bit) != 0) ++bad;
        seen |= bit;
    }
    if (std::abs(sum - 1.0) > 1e-12) ++bad;
    nodes += static_cast<long long>(outcomes.size());
    return bad;
}

State checked_to_chance(const RhodeIslandGame& game, State state, int& bad) {
    state = game.apply_action(state, kActionCallCheck);
    state = game.apply_action(state, kActionCallCheck);
    if (!game.is_chance(state)) ++bad;
    return state;
}

// Gilpin & Sandholm, J. ACM 2007: one node per deal prefix,
// 1 + 52 + 52*51 + 52*51*50 + 52*51*50*49.
struct SignalCount {
    long long nodes;
    int bad;
};

SignalCount signal_tree(const RhodeIslandGame& game) {
    SignalCount tally{1, 0};
    State root = game.initial_state();
    tally.bad += note_chance(game, root, tally.nodes);
    for (const ChanceOutcome& hole0 : game.chance_outcomes(root)) {
        State one = game.apply_action(root, hole0.action);
        tally.bad += note_chance(game, one, tally.nodes);
        for (const ChanceOutcome& hole1 : game.chance_outcomes(one)) {
            State flop = checked_to_chance(
                game, game.apply_action(one, hole1.action), tally.bad);
            if (!game.is_chance(flop)) continue;
            tally.bad += note_chance(game, flop, tally.nodes);
            for (const ChanceOutcome& board : game.chance_outcomes(flop)) {
                State turn = checked_to_chance(
                    game, game.apply_action(flop, board.action), tally.bad);
                if (!game.is_chance(turn)) continue;
                tally.bad += note_chance(game, turn, tally.nodes);
            }
        }
    }
    return tally;
}

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

TEST_CASE("RIH signal tree node count matches the paper") {
    // Every deal prefix, with dealt cards excluded and each chance
    // node a distribution. The total is the published 6,632,705.
    RhodeIslandGame game;
    SignalCount tally = signal_tree(game);
    REQUIRE(tally.bad == 0);
    REQUIRE(tally.nodes == 6632705LL);
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
    State state = play(game, deal_two(game, 0, 4),
                       {kActionCallCheck, kActionRaise, kActionFold});
    REQUIRE(game.is_terminal(state));
    REQUIRE(state.contribution[0] + state.contribution[1] == 20);
    REQUIRE(game.terminal_utility(state, 0) == -5.0);
    REQUIRE(game.terminal_utility(state, 1) == 5.0);
}

TEST_CASE(
    "rhode island showdown: a straight beats a flush, unlike standard poker") {
    RhodeIslandGame game;
    State state = deal_two(game, make_card(8, 2), make_card(3, 0));
    state = play(game, state, {kActionCallCheck, kActionCallCheck},
                 make_card(4, 2));
    state = play(game, state, {kActionCallCheck, kActionCallCheck},
                 make_card(5, 2));
    state = play(game, state, {kActionCallCheck, kActionCallCheck});
    REQUIRE(game.is_terminal(state));
    REQUIRE(state.contribution[0] + state.contribution[1] == 10);
    REQUIRE(game.terminal_utility(state, 1) == 5.0);
    REQUIRE(game.terminal_utility(state, 0) == -5.0);
}

TEST_CASE("rhode island showdown: equal-ranked hands split the pot") {
    RhodeIslandGame game;
    State state = deal_two(game, make_card(4, 2), make_card(4, 3));
    state = play(game, state, {kActionCallCheck, kActionCallCheck},
                 make_card(1, 0));
    state = play(game, state, {kActionCallCheck, kActionCallCheck},
                 make_card(8, 1));
    state = play(game, state, {kActionCallCheck, kActionCallCheck});
    REQUIRE(game.is_terminal(state));
    REQUIRE(game.terminal_utility(state, 0) == 0.0);
    REQUIRE(game.terminal_utility(state, 1) == 0.0);
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
        auto [entry, inserted] =
            label_to_index.emplace(rih_infoset_label(s), game.infoset_index(s));
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

    REQUIRE(rih_infoset_label(preflop_state) ==
            rih_infoset_label(preflop_state_other_opponent_hole));
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

    REQUIRE(rih_infoset_label(postflop_state) ==
            rih_infoset_label(postflop_state_other_opponent_hole));
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
    require_showdown_pot(game, state, [](const State& terminal) {
        return terminal.contribution;
    });
}
namespace {
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
