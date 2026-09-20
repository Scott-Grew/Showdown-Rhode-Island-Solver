// Leduc rules built on betting_round.hpp, plus the dense infoset
// index the solver tables are addressed by.

#include "game/leduc.hpp"

#include <array>
#include <cstddef>
#include <string>

#include "game/betting_round.hpp"

namespace cfr::game {

// Betting rounds and cards dealt in one full hand.
constexpr int kLeducRoundCount = 2;
constexpr int kLeducCardsDealt = 3;

static_assert(max_history_depth(kLeducRoundCount, kLeducMaxRaisesPerRound,
                                kLeducCardsDealt) <=
              static_cast<int>(kMaxHistory));

namespace {

// Deck shape: a card id is rank * kLeducSuitCount + suit.
constexpr int kLeducSuitCount = 2;
constexpr int kLeducCardRankCount = 3;

// Rank 0..2 of a Leduc card id.
int leduc_card_rank(int card) {
    return card / kLeducSuitCount;
}

// Showdown winner, or kNoWinner on a tie. Pairing the board beats
// any unpaired hand, then the higher rank wins.
Player showdown_winner(const State& state) {
    int board_rank = leduc_card_rank(state.board_cards[0]);
    int rank0 = leduc_card_rank(state.hole_cards[0]);
    int rank1 = leduc_card_rank(state.hole_cards[1]);
    bool pair0 = rank0 == board_rank;
    bool pair1 = rank1 == board_rank;
    if (pair0 != pair1) return pair0 ? 0 : 1;
    if (rank0 != rank1) return rank0 > rank1 ? 0 : 1;
    return kNoWinner;
}

// Rank letter for infoset labels; suits are not shown.
std::string card_name(int card) {
    static const char* const kRankNames[] = {"J", "Q", "K"};
    return kRankNames[leduc_card_rank(card)];
}

// Infoset table dimensions: round_progress values at a decision,
// and the ways round 1 can end without a fold.
constexpr int kLeducRoundProgressCount =
    round_progress_count(kLeducMaxRaisesPerRound);
constexpr int kLeducFinalProgressCount = kLeducRoundProgressCount - 1;

// True once the board card is out.
bool board_card_dealt(const ParsedRounds& parsed) {
    return parsed.board_cards_dealt >= 1;
}

}

// Ante plus the bets of each round played so far.
std::array<int, 2> leduc_contributions(const State& state) {
    std::array<int, 2> contributions = {kLeducAnte, kLeducAnte};
    ParsedRounds parsed = parse_rounds(state);
    apply_round_contributions(parsed.round(0), kLeducRound1Bet, contributions);
    if (board_card_dealt(parsed)) {
        apply_round_contributions(parsed.round(1), kLeducRound2Bet,
                                  contributions);
    }
    return contributions;
}

// A default State: no cards and an empty history.
State LeducGame::initial_state() const {
    return State{};
}

// True while a hole card is missing, or when round 1 has closed
// and the board card is not out yet.
bool LeducGame::is_chance(const State& state) const {
    if (state.hole_cards[0] == -1 || state.hole_cards[1] == -1) return true;
    ParsedRounds parsed = parse_rounds(state);
    if (board_card_dealt(parsed)) return false;
    return round_closed(parsed.round(0));
}

// A fold ends the hand in either round; otherwise it ends when
// round 2 closes.
bool LeducGame::is_terminal(const State& state) const {
    if (is_chance(state)) return false;
    ParsedRounds parsed = parse_rounds(state);
    if (!board_card_dealt(parsed)) return folded(parsed.round(0));
    return folded(parsed.round(1)) || round_closed(parsed.round(1));
}

// Player to act in the active round, or -1 at a chance node.
Player LeducGame::current_player(const State& state) const {
    if (is_chance(state)) return kChancePlayer;
    ParsedRounds parsed = parse_rounds(state);
    return round_acting_player(active_round_actions(parsed));
}

// Legal actions of the active round under the raise cap.
std::vector<Action> LeducGame::legal_actions(const State& state) const {
    ParsedRounds parsed = parse_rounds(state);
    return round_legal_actions(active_round_actions(parsed),
                               kLeducMaxRaisesPerRound);
}

// Appends the entry, and at a chance node also stores the card
// decoded from action - kChanceCardOffset.
State LeducGame::apply_action(const State& state, Action action) const {
    State next = state;
    bool dealing = is_chance(state);
    append_action(next, action);
    if (dealing) deal_card(next, action - kChanceCardOffset);
    return next;
}

// A fold pays the other player; otherwise the showdown
// decides, and a tie pays zero.
double LeducGame::terminal_utility(const State& state, Player player) const {
    std::array<int, 2> contributions = leduc_contributions(state);
    ParsedRounds parsed = parse_rounds(state);
    std::span<const Action> final_round = active_round_actions(parsed);

    if (folded(final_round)) {
        Player folder = static_cast<Player>((final_round.size() - 1) % 2);
        return settle(contributions, 1 - folder, player);
    }

    return settle(contributions, showdown_winner(state), player);
}

// Builds "P<player>:<card>|<board>:<round 1>;<round 2>".
InfosetLabel LeducGame::infoset_label(const State& state) const {
    Player acting_player = current_player(state);
    ParsedRounds parsed = parse_rounds(state);

    std::string label = "P" + std::to_string(acting_player) + ":" +
                        card_name(state.hole_cards[acting_player]);
    if (state.board_count != 0) {
        label += "|" + card_name(state.board_cards[0]);
    }
    label += ":";
    for (Action action : parsed.round(0)) label += action_name(action) + ",";
    label += ";";
    for (Action action : parsed.round(1)) label += action_name(action) + ",";
    return label;
}

// Round 1 rows plus round 2 rows; infoset_index has the layout.
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
    ParsedRounds parsed = parse_rounds(state);
    int hole_rank = leduc_card_rank(state.hole_cards[acting_player]);

    if (!board_card_dealt(parsed)) {
        int round1_progress = round_progress(parsed.round(0));
        return static_cast<std::uint32_t>(
            round1_progress * kLeducCardRankCount + hole_rank);
    }

    // A closed round has progress 1..5, shifted here to 0..4.
    int round1_final_progress = round_progress(parsed.round(0)) - 1;
    int round2_progress = round_progress(parsed.round(1));
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

// Uniform over the undealt cards of the six-card deck.
std::vector<ChanceOutcome> LeducGame::chance_outcomes(
    const State& state) const {
    return deal_outcomes(state, kLeducDeckSize);
}

}
