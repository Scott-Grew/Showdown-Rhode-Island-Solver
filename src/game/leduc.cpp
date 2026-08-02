#include "game/leduc.hpp"

#include <array>
#include <cstddef>
#include <string>

#include "game/betting_round.hpp"

namespace cfr::game {

static_assert(max_history_depth(2, kMaxRaisesPerRound, 3) <= static_cast<int>(kMaxHistory));

namespace {

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

std::string card_name(int card) {
    static const char* const kRankNames[] = {"J", "Q", "K"};
    return kRankNames[card / 2];
}

constexpr int kLeducCardRankCount = 3;
constexpr int kLeducRoundStageCount = 6;
constexpr int kLeducRound1EndingCount = 5;

int card_rank(int card) {
    return card / 2;
}

bool public_card_dealt(const ParsedRounds& parsed) {
    return parsed.board_cards_dealt >= 1;
}

}

std::array<int, 2> leduc_contributions(const State& state) {
    std::array<int, 2> contribution = {kLeducAnte, kLeducAnte};
    ParsedRounds parsed = parse_rounds(state);
    apply_round_contributions(parsed.round(0), kRound1Bet, contribution);
    if (public_card_dealt(parsed)) {
        apply_round_contributions(parsed.round(1), kRound2Bet, contribution);
    }
    return contribution;
}

State LeducGame::initial_state() const {
    return State{};
}

bool LeducGame::is_chance(const State& state) const {
    if (state.private_cards[0] == -1 || state.private_cards[1] == -1) return true;
    ParsedRounds parsed = parse_rounds(state);
    if (public_card_dealt(parsed)) return false;
    return round_closed(parsed.round(0));
}

bool LeducGame::is_terminal(const State& state) const {
    if (is_chance(state)) return false;
    ParsedRounds parsed = parse_rounds(state);
    if (!public_card_dealt(parsed)) return folded(parsed.round(0));
    return folded(parsed.round(1)) || round_closed(parsed.round(1));
}

Player LeducGame::current_player(const State& state) const {
    if (is_chance(state)) return -1;
    ParsedRounds parsed = parse_rounds(state);
    return round_actor(active_round_actions(parsed));
}

std::vector<Action> LeducGame::legal_actions(const State& state) const {
    ParsedRounds parsed = parse_rounds(state);
    return round_legal_actions(active_round_actions(parsed), kMaxRaisesPerRound);
}

State LeducGame::apply_action(const State& state, Action action) const {
    State next = state;
    bool dealing = is_chance(state);
    append_action(next, action);
    if (dealing) deal_card(next, action - kChanceCardOffset);
    return next;
}

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

std::uint32_t LeducGame::infoset_count() const {
    return static_cast<std::uint32_t>(kLeducRoundStageCount * kLeducCardRankCount +
                                       kLeducRound1EndingCount * kLeducRoundStageCount * kLeducCardRankCount *
                                           kLeducCardRankCount);
}

std::uint32_t LeducGame::infoset_index(const State& state) const {
    Player player = current_player(state);
    ParsedRounds parsed = parse_rounds(state);
    int private_rank = card_rank(state.private_cards[player]);

    if (!public_card_dealt(parsed)) {
        int round1_stage = round_progress(parsed.round(0));
        return static_cast<std::uint32_t>(round1_stage * kLeducCardRankCount + private_rank);
    }

    int round1_ending = round_progress(parsed.round(0)) - 1;
    int round2_stage = round_progress(parsed.round(1));
    int public_rank = card_rank(state.public_cards[0]);

    int round2_offset =
        ((round1_ending * kLeducRoundStageCount + round2_stage) * kLeducCardRankCount + private_rank) *
            kLeducCardRankCount +
        public_rank;
    return static_cast<std::uint32_t>(kLeducRoundStageCount * kLeducCardRankCount + round2_offset);
}

std::vector<std::pair<Action, double>> LeducGame::chance_outcomes(const State& state) const {
    return deal_outcomes(state, kLeducDeckSize);
}

}
