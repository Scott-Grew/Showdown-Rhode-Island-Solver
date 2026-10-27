// Rhode Island rules built on betting_round.hpp, showdowns from
// eval3, and the GameShrink infoset index. A completed round is
// stored as its closing sequence, and the cards as their class, so
// each real infoset owns one row.

#include "game/rhode_island.hpp"

#include <array>
#include <cassert>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <string>

#include "abstraction/rih_gameshrink.hpp"
#include "eval/eval3.hpp"
#include "game/betting_round.hpp"
#include "game/card.hpp"
#include "game/game.hpp"

namespace cfr::game {

// Betting rounds and cards dealt in one full hand.
constexpr int kRihRoundCount = 3;
constexpr int kRihCardsDealt = 4;

static_assert(GameLike<RhodeIslandGame>);
static_assert(max_history_depth(kRihRoundCount, kRihMaxRaisesPerRound,
                                kRihCardsDealt) <=
              static_cast<int>(kMaxHistory));

std::string rih_card_name(int card) {
    static const char* const kRankNames[] = {"2", "3", "4", "5", "6", "7", "8",
                                             "9", "T", "J", "Q", "K", "A"};
    static const char* const kSuitNames[] = {"c", "d", "h", "s"};
    Card typed_card = static_cast<Card>(card);
    return std::string(kRankNames[card_rank(typed_card)]) +
           kSuitNames[card_suit(typed_card)];
}

namespace {

// Winner from the ranks stored when the turn was dealt, or
// std::nullopt on a tie.
std::optional<Player> showdown_winner(const State& state) {
    if (state.showdown_rank[0] == state.showdown_rank[1]) return std::nullopt;
    return state.showdown_rank[0] > state.showdown_rank[1] ? 0 : 1;
}

// Both players' three-card ranks, stored once when the turn card
// arrives. Later showdowns of this deal read the fields.
void remember_showdown_ranks(State& state) {
    std::array<Card, 3> hand0 = {static_cast<Card>(state.hole_cards[0]),
                                 static_cast<Card>(state.board_cards[0]),
                                 static_cast<Card>(state.board_cards[1])};
    std::array<Card, 3> hand1 = {static_cast<Card>(state.hole_cards[1]),
                                 static_cast<Card>(state.board_cards[0]),
                                 static_cast<Card>(state.board_cards[1])};
    state.showdown_rank[0] = cfr::eval::lookup_3card(hand0.data());
    state.showdown_rank[1] = cfr::eval::lookup_3card(hand1.data());
}

// In-round progress codes, and closing sequences of a finished round.
constexpr int kRihRoundProgressCount =
    round_progress_count(kRihMaxRaisesPerRound);
constexpr int kRihClosingCount = closing_sequence_count(kRihMaxRaisesPerRound);
static_assert(kRihRoundProgressCount == 8);
static_assert(kRihClosingCount == 7);

// One partition per process. The lossless refinement of the authors'
// partition is built once; every RhodeIslandGame refers to it.
const cfr::abstraction::RihPartition& shared_partition() {
    static const cfr::abstraction::RihPartition partition =
        cfr::abstraction::RihPartition::compute().refined();
    return partition;
}

// GameShrink class of the acting player's own cards. The span length
// is the round: hole, hole and flop, or all three.
std::uint16_t own_class(const cfr::abstraction::RihPartition& partition,
                        const State& state, Player player) {
    std::array<Card, 3> cards{};
    cards[0] = static_cast<Card>(state.hole_cards[player]);
    std::size_t count = 1;
    if (state.board_count >= 1) {
        cards[count++] = static_cast<Card>(state.board_cards[0]);
    }
    if (state.board_count >= 2) {
        cards[count++] = static_cast<Card>(state.board_cards[1]);
    }
    return partition.class_of(std::span<const Card>(cards.data(), count));
}

// Each player's refined class of the cards dealt so far. A player
// with no hole card yet has nothing to store.
void remember_own_classes(const cfr::abstraction::RihPartition& partition,
                          State& state) {
    if (state.hole_cards[0] >= 0)
        state.card_class[0] = own_class(partition, state, 0);
    if (state.hole_cards[1] >= 0)
        state.card_class[1] = own_class(partition, state, 1);
}

}

RhodeIslandGame::RhodeIslandGame() : partition_(&shared_partition()) {}

// Antes are posted before any card is dealt.
State RhodeIslandGame::initial_state() const {
    State state;
    state.contribution[0] = kRihAnte;
    state.contribution[1] = kRihAnte;
    return state;
}

bool RhodeIslandGame::is_chance(const State& state) const {
    if (state.hole_cards[0] == -1 || state.hole_cards[1] == -1) return true;
    if (state.board_count >= 2) return false;
    return round_closed(state);
}

bool RhodeIslandGame::is_terminal(const State& state) const {
    if (is_chance(state)) return false;
    if (state.board_count < 2) return folded(state);
    return folded(state) || round_closed(state);
}

Player RhodeIslandGame::current_player(const State& state) const {
    assert(!is_chance(state) && !is_terminal(state));
    return static_cast<std::size_t>(state.round_len) % kPlayerCount;
}

FixedVector<Action, RhodeIslandGame::kMaxActions>
RhodeIslandGame::legal_actions(const State& state) const {
    return round_legal_actions<kMaxActions>(state, kRihMaxRaisesPerRound);
}

int bet_size_of(const State& state) {
    if (state.board_count == 0) return kRihRound1Bet;
    if (state.board_count == 1) return kRihRound2Bet;
    return kRihRound3Bet;
}

// Appends the entry. A dealt card is stored, and a board card opens
// the next round. The turn card also stores both showdown ranks.
// A betting action updates the round and the pot.
State RhodeIslandGame::apply_action(const State& state, Action action) const {
    State next = state;
    bool dealing = is_chance(state);
    append_action(next, action);
    if (dealing) {
        deal_and_advance(next, action - kChanceCardOffset,
                         kRihMaxRaisesPerRound);
        remember_own_classes(*partition_, next);
        if (next.board_count == 2) remember_showdown_ranks(next);
        return next;
    }
    apply_betting_action(next, action, bet_size_of(next));
    return next;
}

double RhodeIslandGame::terminal_utility(const State& state,
                                         Player player) const {
    if (folded(state)) {
        Player folder =
            (static_cast<std::size_t>(state.round_len) - 1) % kPlayerCount;
        return settle(state.contribution, opponent(folder), player);
    }
    return settle(state.contribution, showdown_winner(state), player);
}

// "P<player>:<hole>|<board>:<r1>;<r2>;<r3>". Names stay finer than
// the GameShrink index. Each round's text is fixed by its counters.
InfosetLabel RhodeIslandGame::infoset_label(const State& state) const {
    Player acting_player = current_player(state);
    std::string label = "P" + std::to_string(acting_player) + ":" +
                        rih_card_name(state.hole_cards[acting_player]);
    for (std::uint8_t index = 0; index < state.board_count; ++index)
        label += "|" + rih_card_name(state.board_cards[index]);
    label += ":";
    for (int round = 0; round < kRihRoundCount; ++round) {
        if (round > 0) label += ";";
        if (round < state.board_count) {
            int code = state.closing[static_cast<std::size_t>(round)];
            append_round_actions(
                label, closing_raises(code, kRihMaxRaisesPerRound),
                closing_opened_with_check(code, kRihMaxRaisesPerRound), true);
        } else if (round == state.board_count) {
            append_round_actions(label, state.raises_used,
                                 opened_with_check(state), false);
        }
    }
    return label;
}

// Rows of the three rounds. P = 8 progress codes, S = 7 closing
// sequences, Cr the GameShrink class count of round r:
//
//   round 0 | P * C0
//   round 1 | S * P * C1
//   round 2 | S * S * P * C2
//
// With the refined class counts this is 3,142,152.
std::uint32_t RhodeIslandGame::infoset_count() const {
    constexpr std::uint32_t kProgress =
        static_cast<std::uint32_t>(kRihRoundProgressCount);
    constexpr std::uint32_t kClosing =
        static_cast<std::uint32_t>(kRihClosingCount);
    std::uint32_t class0 = partition_->class_count(0);
    std::uint32_t class1 = partition_->class_count(1);
    std::uint32_t class2 = partition_->class_count(2);
    return kProgress * class0 + kClosing * kProgress * class1 +
           kClosing * kClosing * kProgress * class2;
}

// Round r of the acting player, with R0 = P*C0 and R1 = S*P*C1:
//
//   round 0 | progress * C0 + class(hole)
//   round 1 | R0 + (closing0 * P + progress) * C1 + class(hole, flop)
//   round 2 | R0 + R1
//           |   + ((closing0 * S + closing1) * P + progress) * C2
//           |   + class(hole, flop, turn)
//
// progress is the active round. closingR was stored when round R
// closed. The class was stored when the latest card was dealt.
std::uint32_t RhodeIslandGame::infoset_index(const State& state) const {
    Player acting_player = current_player(state);
    std::uint32_t progress = static_cast<std::uint32_t>(round_progress(state));
    std::uint32_t card_class = state.card_class[acting_player];
    assert(card_class == own_class(*partition_, state, acting_player));
    constexpr std::uint32_t kProgress =
        static_cast<std::uint32_t>(kRihRoundProgressCount);
    constexpr std::uint32_t kClosing =
        static_cast<std::uint32_t>(kRihClosingCount);

    if (state.board_count == 0) {
        return progress * partition_->class_count(0) + card_class;
    }

    std::uint32_t round0_rows = kProgress * partition_->class_count(0);
    std::uint32_t closing0 = state.closing[0];
    if (state.board_count == 1) {
        std::uint32_t betting = closing0 * kProgress + progress;
        return round0_rows + betting * partition_->class_count(1) + card_class;
    }

    std::uint32_t round1_rows =
        kClosing * kProgress * partition_->class_count(1);
    std::uint32_t closing1 = state.closing[1];
    std::uint32_t betting =
        (closing0 * kClosing + closing1) * kProgress + progress;
    return round0_rows + round1_rows + betting * partition_->class_count(2) +
           card_class;
}

FixedVector<ChanceOutcome, kMaxChanceOutcomes> RhodeIslandGame::chance_outcomes(
    const State& state) const {
    return deal_outcomes(state, kCardCount);
}

void RhodeIslandGame::set_hole_card(State& state, Player player,
                                    int card) const {
    assert(player < kPlayerCount);
    assert(card >= 0 && card < kCardCount);
    state.hole_cards[player] = static_cast<std::int8_t>(card);
    state.card_class[player] = own_class(*partition_, state, player);
}

}
