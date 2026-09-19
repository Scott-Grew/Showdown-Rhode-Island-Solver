#include "game/leduc.hpp"

#include <array>
#include <cstddef>
#include <string>

#include "game/betting_round.hpp"

namespace cfr::game {

static_assert(max_history_depth(2, kMaxRaisesPerRound, 3) <= static_cast<int>(kMaxHistory));

namespace {

// Showdown from player 0's side: 1 win, -1 loss, 0 tie. Pairing
// the board beats any unpaired hand, then the higher rank wins.
int compare_hands(const State& state) {
    int public_rank = state.public_cards[0] / 2;
    int rank0 = state.private_cards[0] / 2;
    int rank1 = state.private_cards[1] / 2;
    bool pair0 = rank0 == public_rank;
    bool pair1 = rank1 == public_rank;
    if (pair0 != pair1) return pair0 ? 1 : -1;
    if (rank0 != rank1) return rank0 > rank1 ? 1 : -1;
    return 0;
}

// Rank letter for infoset labels; suits are not shown.
std::string card_name(int card) {
    static const char* const kRankNames[] = {"J", "Q", "K"};
    return kRankNames[card / 2];
}

// Infoset table dimensions: ranks, round_progress values at a
// decision, and the ways round 1 can end without a fold.
constexpr int kLeducCardRankCount = 3;
constexpr int kLeducRoundStageCount = 6;
constexpr int kLeducRound1EndingCount = 5;

// Rank 0..2 of a Leduc card id.
int card_rank(int card) {
    return card / 2;
}

// True once the board card is out.
bool public_card_dealt(const ParsedRounds& parsed) {
    return parsed.board_cards_dealt >= 1;
}

}

// Ante plus the bets of each round played so far.
std::array<int, 2> leduc_contributions(const State& state) {
    std::array<int, 2> contribution = {kLeducAnte, kLeducAnte};
    ParsedRounds parsed = parse_rounds(state);
    apply_round_contributions(parsed.round(0), kRound1Bet, contribution);
    if (public_card_dealt(parsed)) {
        apply_round_contributions(parsed.round(1), kRound2Bet, contribution);
    }
    return contribution;
}

// The empty state, before any card is dealt.
State LeducGame::initial_state() const {
    return State{};
}

// True while a hole card is missing, or when round 1 has closed
// and the board card is not out yet.
bool LeducGame::is_chance(const State& state) const {
    if (state.private_cards[0] == -1 || state.private_cards[1] == -1) return true;
    ParsedRounds parsed = parse_rounds(state);
    if (public_card_dealt(parsed)) return false;
    return round_closed(parsed.round(0));
}

// A fold ends the hand in either round; otherwise it ends when
// round 2 closes.
bool LeducGame::is_terminal(const State& state) const {
    if (is_chance(state)) return false;
    ParsedRounds parsed = parse_rounds(state);
    if (!public_card_dealt(parsed)) return folded(parsed.round(0));
    return folded(parsed.round(1)) || round_closed(parsed.round(1));
}

// Player to act in the active round, or -1 at a chance node.
Player LeducGame::current_player(const State& state) const {
    if (is_chance(state)) return -1;
    ParsedRounds parsed = parse_rounds(state);
    return round_actor(active_round_actions(parsed));
}

// Legal actions of the active round under the raise cap.
std::vector<Action> LeducGame::legal_actions(const State& state) const {
    ParsedRounds parsed = parse_rounds(state);
    return round_legal_actions(active_round_actions(parsed), kMaxRaisesPerRound);
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
    std::array<int, 2> contribution = leduc_contributions(state);
    ParsedRounds parsed = parse_rounds(state);
    std::span<const Action> final_round = active_round_actions(parsed);

    if (folded(final_round)) {
        Player folder = static_cast<Player>((final_round.size() - 1) % 2);
        return settle(contribution, 1 - folder, player);
    }

    int showdown_result = compare_hands(state);
    return settle(contribution, showdown_result == 0 ? -1 : (showdown_result > 0 ? 0 : 1), player);
}

// Builds "P<player>:<card>|<board>:<round 1>;<round 2>".
InfoSetKey LeducGame::infoset_label(const State& state) const {
    Player player = current_player(state);
    ParsedRounds parsed = parse_rounds(state);

    std::string key = "P" + std::to_string(player) + ":" + card_name(state.private_cards[player]);
    if (state.public_count != 0) {
        key += "|" + card_name(state.public_cards[0]);
    }
    key += ":";
    for (Action action : parsed.round(0)) key += action_name(action) + ",";
    key += ";";
    for (Action action : parsed.round(1)) key += action_name(action) + ",";
    return key;
}

// Round 1 rows plus round 2 rows; infoset_index has the layout.
std::uint32_t LeducGame::infoset_count() const {
    return static_cast<std::uint32_t>(kLeducRoundStageCount * kLeducCardRankCount +
                                       kLeducRound1EndingCount * kLeducRoundStageCount * kLeducCardRankCount *
                                           kLeducCardRankCount);
}

// Round 1 rows are progress * ranks + private rank. Round 2 rows
// follow, keyed by round 1 ending, progress and both ranks.
std::uint32_t LeducGame::infoset_index(const State& state) const {
    Player player = current_player(state);
    ParsedRounds parsed = parse_rounds(state);
    int private_rank = card_rank(state.private_cards[player]);

    if (!public_card_dealt(parsed)) {
        int round1_stage = round_progress(parsed.round(0));
        return static_cast<std::uint32_t>(round1_stage * kLeducCardRankCount + private_rank);
    }

    // A closed round has progress 1..5, shifted here to 0..4.
    int round1_ending = round_progress(parsed.round(0)) - 1;
    int round2_stage = round_progress(parsed.round(1));
    int public_rank = card_rank(state.public_cards[0]);

    int round2_offset =
        ((round1_ending * kLeducRoundStageCount + round2_stage) * kLeducCardRankCount + private_rank) *
            kLeducCardRankCount +
        public_rank;
    return static_cast<std::uint32_t>(kLeducRoundStageCount * kLeducCardRankCount + round2_offset);
}

// Uniform over the undealt cards of the six-card deck.
std::vector<std::pair<Action, double>> LeducGame::chance_outcomes(const State& state) const {
    return deal_outcomes(state, kLeducDeckSize);
}

}
