#include "oracles/leduc.hpp"

#include <array>
#include <cassert>
#include <cstddef>
#include <optional>
#include <string>

#include "game/betting_round.hpp"

namespace cfr::game {

constexpr int kLeducRoundCount = 2;
constexpr int kLeducCardsDealt = 3;

static_assert(max_history_depth(kLeducRoundCount, kLeducMaxRaisesPerRound,
                                kLeducCardsDealt) <=
              static_cast<int>(kMaxHistory));

namespace {

// Deck shape: a card id is rank * kLeducSuitCount + suit.
constexpr int kLeducSuitCount = 2;
constexpr int kLeducCardRankCount = 3;

int leduc_card_rank(int card) {
    return card / kLeducSuitCount;
}

// Showdown winner, or std::nullopt on a tie. Pairing the board beats
// any unpaired hand, then the higher rank wins.
std::optional<Player> showdown_winner(const State& state) {
    int board_rank = leduc_card_rank(state.board_cards[0]);
    int rank0 = leduc_card_rank(state.hole_cards[0]);
    int rank1 = leduc_card_rank(state.hole_cards[1]);
    bool pair0 = rank0 == board_rank;
    bool pair1 = rank1 == board_rank;
    if (pair0 != pair1) return pair0 ? 0 : 1;
    if (rank0 != rank1) return rank0 > rank1 ? 0 : 1;
    return std::nullopt;
}

std::string card_name(int card) {
    static const char* const kRankNames[] = {"J", "Q", "K"};
    return kRankNames[leduc_card_rank(card)];
}

// Infoset table dimensions: round_progress values at a decision,
// and the ways round 1 can end without a fold.
constexpr int kLeducRoundProgressCount =
    round_progress_count(kLeducMaxRaisesPerRound);
constexpr int kLeducFinalProgressCount = kLeducRoundProgressCount - 1;

}

// Antes are posted before any card is dealt.
State LeducGame::initial_state() const {
    State state;
    state.contribution[0] = kLeducAnte;
    state.contribution[1] = kLeducAnte;
    return state;
}

bool LeducGame::is_chance(const State& state) const {
    if (state.hole_cards[0] == -1 || state.hole_cards[1] == -1) return true;
    if (state.board_count >= 1) return false;
    return round_closed(state);
}

// A fold ends the hand in either round; otherwise it ends when
// round 2 closes.
bool LeducGame::is_terminal(const State& state) const {
    if (is_chance(state)) return false;
    if (state.board_count < 1) return folded(state);
    return folded(state) || round_closed(state);
}

Player LeducGame::current_player(const State& state) const {
    assert(!is_chance(state) && !is_terminal(state));
    return static_cast<std::size_t>(state.round_len) % kPlayerCount;
}

FixedVector<Action, LeducGame::kMaxActions> LeducGame::legal_actions(
    const State& state) const {
    return round_legal_actions<kMaxActions>(state, kLeducMaxRaisesPerRound);
}

State LeducGame::apply_action(const State& state, Action action) const {
    State next = state;
    bool dealing = is_chance(state);
    append_action(next, action);
    if (dealing) {
        deal_and_advance(next, action - kChanceCardOffset,
                         kLeducMaxRaisesPerRound);
        return next;
    }
    int bet_size = next.board_count == 0 ? kLeducRound1Bet : kLeducRound2Bet;
    apply_betting_action(next, action, bet_size);
    return next;
}

double LeducGame::terminal_utility(const State& state, Player player) const {
    if (folded(state)) {
        Player folder =
            (static_cast<std::size_t>(state.round_len) - 1) % kPlayerCount;
        return settle(state.contribution, opponent(folder), player);
    }
    return settle(state.contribution, showdown_winner(state), player);
}

InfosetLabel LeducGame::infoset_label(const State& state) const {
    Player acting_player = current_player(state);
    std::string label = "P" + std::to_string(acting_player) + ":" +
                        card_name(state.hole_cards[acting_player]);
    if (state.board_count != 0) label += "|" + card_name(state.board_cards[0]);
    label += ":";
    if (state.board_count == 0) {
        append_round_actions(label, state.raises_used, opened_with_check(state),
                             false);
    } else {
        int code = state.closing[0];
        append_round_actions(
            label, closing_raises(code, kLeducMaxRaisesPerRound),
            closing_opened_with_check(code, kLeducMaxRaisesPerRound), true);
    }
    label += ";";
    if (state.board_count != 0)
        append_round_actions(label, state.raises_used, opened_with_check(state),
                             false);
    return label;
}

// 288 = 6 * 3 + 5 * 6 * 3 * 3. Round 1 can end from every progress
// value except the empty round, hence one fewer endings than values.
std::uint32_t LeducGame::infoset_count() const {
    return static_cast<std::uint32_t>(
        kLeducRoundProgressCount * kLeducCardRankCount +
        kLeducFinalProgressCount * kLeducRoundProgressCount *
            kLeducCardRankCount * kLeducCardRankCount);
}

// Round 1 rows are progress * ranks + hole rank. Round 2 rows
// follow, keyed by round 1 ending, progress and both ranks.
std::uint32_t LeducGame::infoset_index(const State& state) const {
    Player acting_player = current_player(state);
    int hole_rank = leduc_card_rank(state.hole_cards[acting_player]);

    if (state.board_count == 0) {
        return static_cast<std::uint32_t>(
            round_progress(state) * kLeducCardRankCount + hole_rank);
    }

    // Closed-round progress is 1..5; the row stores that minus one.
    int code = state.closing[0];
    int raises = closing_raises(code, kLeducMaxRaisesPerRound);
    bool opened = closing_opened_with_check(code, kLeducMaxRaisesPerRound);
    int round1_final_progress = raises * 2 + (opened ? 1 : 0) - 1;
    int round2_progress = round_progress(state);
    int board_rank = leduc_card_rank(state.board_cards[0]);

    int round2_offset =
        ((round1_final_progress * kLeducRoundProgressCount + round2_progress) *
             kLeducCardRankCount +
         hole_rank) *
            kLeducCardRankCount +
        board_rank;
    return static_cast<std::uint32_t>(
        kLeducRoundProgressCount * kLeducCardRankCount + round2_offset);
}

FixedVector<ChanceOutcome, kMaxChanceOutcomes> LeducGame::chance_outcomes(
    const State& state) const {
    return deal_outcomes(state, kLeducDeckSize);
}

}
