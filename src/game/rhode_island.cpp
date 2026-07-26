#include "game/rhode_island.hpp"

#include <algorithm>
#include <array>
#include <cstddef>
#include <string>

#include "eval/eval3.hpp"
#include "game/card.hpp"
#include "game/game_concept.hpp"

namespace cfr::game {

static_assert(GameLike<RhodeIslandGame>);

namespace {

struct ParsedHistory {
    std::vector<Action> round1_actions;
    std::vector<Action> round2_actions;
    std::vector<Action> round3_actions;
    int board_cards_dealt = 0;
};

ParsedHistory parse_history(const State& state) {
    ParsedHistory parsed;
    int chance_events_seen = 0;
    for (std::uint8_t index = 0; index < state.history_len; ++index) {
        Action entry = state.history[index];
        if (entry >= kRihChanceCardOffset) {
            ++chance_events_seen;
            continue;
        }
        if (chance_events_seen < 3) {
            parsed.round1_actions.push_back(entry);
        } else if (chance_events_seen < 4) {
            parsed.round2_actions.push_back(entry);
        } else {
            parsed.round3_actions.push_back(entry);
        }
    }
    parsed.board_cards_dealt = chance_events_seen >= 2 ? chance_events_seen - 2 : 0;
    return parsed;
}

const std::vector<Action>& active_round_actions(const ParsedHistory& parsed) {
    if (parsed.board_cards_dealt == 0) return parsed.round1_actions;
    if (parsed.board_cards_dealt == 1) return parsed.round2_actions;
    return parsed.round3_actions;
}

bool folded(const std::vector<Action>& round_actions) {
    return !round_actions.empty() && round_actions.back() == kRihActionFold;
}

bool round_closed(const std::vector<Action>& round_actions) {
    if (round_actions.size() < 2) return false;
    if (round_actions.back() != kRihActionCallCheck) return false;
    Action previous = round_actions[round_actions.size() - 2];
    return previous == kRihActionCallCheck || previous == kRihActionRaise;
}

Player round_actor(const std::vector<Action>& round_actions) {
    return static_cast<Player>(round_actions.size() % 2);
}

std::vector<Action> round_legal_actions(const std::vector<Action>& round_actions) {
    if (round_actions.empty()) {
        return {kRihActionCallCheck, kRihActionRaise};
    }
    bool facing_wager = round_actions.back() == kRihActionRaise;
    if (!facing_wager) {
        return {kRihActionCallCheck, kRihActionRaise};
    }
    int raises_used = static_cast<int>(std::count(round_actions.begin(), round_actions.end(), kRihActionRaise));
    if (raises_used < kRihMaxRaisesPerRound) {
        return {kRihActionFold, kRihActionCallCheck, kRihActionRaise};
    }
    return {kRihActionFold, kRihActionCallCheck};
}

void apply_round_contributions(const std::vector<Action>& round_actions, int bet_size,
                                std::array<int, 2>& contribution) {
    std::array<int, 2> street_contribution = {0, 0};
    int level_to_match = 0;
    for (std::size_t i = 0; i < round_actions.size(); ++i) {
        Player actor = static_cast<Player>(i % 2);
        Action action = round_actions[i];
        if (action == kRihActionRaise) {
            level_to_match += bet_size;
            street_contribution[actor] = level_to_match;
        } else if (action == kRihActionCallCheck) {
            street_contribution[actor] = level_to_match;
        }
    }
    contribution[0] += street_contribution[0];
    contribution[1] += street_contribution[1];
}

std::array<int, 2> contributions(const State& state) {
    std::array<int, 2> contribution = {kRihAnte, kRihAnte};
    ParsedHistory parsed = parse_history(state);
    apply_round_contributions(parsed.round1_actions, kRihRound1Bet, contribution);
    if (parsed.board_cards_dealt >= 1) apply_round_contributions(parsed.round2_actions, kRihRound2Bet, contribution);
    if (parsed.board_cards_dealt >= 2) apply_round_contributions(parsed.round3_actions, kRihRound3Bet, contribution);
    return contribution;
}

double showdown_utility(const State& state, Player player, const std::array<int, 2>& contribution, int pot) {
    std::array<Card, 3> hand0 = {static_cast<Card>(state.private_cards[0]), static_cast<Card>(state.public_cards[0]),
                                  static_cast<Card>(state.public_cards[1])};
    std::array<Card, 3> hand1 = {static_cast<Card>(state.private_cards[1]), static_cast<Card>(state.public_cards[0]),
                                  static_cast<Card>(state.public_cards[1])};
    cfr::eval::HandRank rank0 = cfr::eval::evaluate_3card(hand0.data());
    cfr::eval::HandRank rank1 = cfr::eval::evaluate_3card(hand1.data());
    if (rank0 == rank1) return 0.0;
    Player winner = rank0 > rank1 ? 0 : 1;
    return player == winner ? static_cast<double>(pot - contribution[winner])
                             : -static_cast<double>(contribution[player]);
}

std::string card_name(int card) {
    static const char* const kRankNames[] = {"2", "3", "4", "5", "6", "7", "8", "9", "T", "J", "Q", "K", "A"};
    static const char* const kSuitNames[] = {"c", "d", "h", "s"};
    Card typed_card = static_cast<Card>(card);
    return std::string(kRankNames[card_rank(typed_card)]) + kSuitNames[card_suit(typed_card)];
}

std::string action_name(Action action) {
    switch (action) {
        case kRihActionFold: return "fold";
        case kRihActionCallCheck: return "call";
        case kRihActionRaise: return "raise";
    }
    return "?";
}

constexpr int kRihRoundStageCount = 8;

int round_progress(const std::vector<Action>& round_actions) {
    int raises_used = static_cast<int>(std::count(round_actions.begin(), round_actions.end(), kRihActionRaise));
    bool opened_with_check = !round_actions.empty() && round_actions.front() == kRihActionCallCheck;
    return raises_used * 2 + (opened_with_check ? 1 : 0);
}

constexpr std::uint32_t kRihRound1IndexSpan = kRihRoundStageCount * kRihDeckSize;
constexpr std::uint32_t kRihRound2IndexSpan =
    kRihRoundStageCount * kRihRoundStageCount * kRihDeckSize * kRihDeckSize;
constexpr std::uint32_t kRihRound3IndexSpan = kRihRoundStageCount * kRihRoundStageCount * kRihRoundStageCount *
                                               kRihDeckSize * kRihDeckSize * kRihDeckSize;
constexpr std::uint32_t kRihRound2IndexBase = kRihRound1IndexSpan;
constexpr std::uint32_t kRihRound3IndexBase = kRihRound2IndexBase + kRihRound2IndexSpan;

}

State RhodeIslandGame::initial_state() const {
    State state;
    state.private_cards = {-1, -1};
    state.pot = 0;
    return state;
}

bool RhodeIslandGame::is_chance(const State& state) const {
    if (state.private_cards[0] == -1 || state.private_cards[1] == -1) return true;
    ParsedHistory parsed = parse_history(state);
    if (parsed.board_cards_dealt == 0) return round_closed(parsed.round1_actions);
    if (parsed.board_cards_dealt == 1) return round_closed(parsed.round2_actions);
    return false;
}

bool RhodeIslandGame::is_terminal(const State& state) const {
    if (is_chance(state)) return false;
    ParsedHistory parsed = parse_history(state);
    if (parsed.board_cards_dealt == 0) return folded(parsed.round1_actions);
    if (parsed.board_cards_dealt == 1) return folded(parsed.round2_actions);
    return folded(parsed.round3_actions) || round_closed(parsed.round3_actions);
}

Player RhodeIslandGame::current_player(const State& state) const {
    if (is_chance(state)) return -1;
    ParsedHistory parsed = parse_history(state);
    return round_actor(active_round_actions(parsed));
}

std::vector<Action> RhodeIslandGame::legal_actions(const State& state) const {
    ParsedHistory parsed = parse_history(state);
    return round_legal_actions(active_round_actions(parsed));
}

State RhodeIslandGame::apply_action(const State& state, Action action) const {
    State next = state;
    if (is_chance(state)) {
        next.history[next.history_len++] = static_cast<std::uint8_t>(action);
        int card = action - kRihChanceCardOffset;
        if (state.private_cards[0] == -1) {
            next.private_cards[0] = static_cast<std::int8_t>(card);
            next.pot += kRihAnte;
        } else if (state.private_cards[1] == -1) {
            next.private_cards[1] = static_cast<std::int8_t>(card);
            next.pot += kRihAnte;
        } else {
            next.public_cards[next.public_count++] = static_cast<std::int8_t>(card);
        }
        return next;
    }

    next.history[next.history_len++] = static_cast<std::uint8_t>(action);
    std::array<int, 2> contribution = contributions(next);
    next.pot = contribution[0] + contribution[1];
    return next;
}

double RhodeIslandGame::terminal_utility(const State& state, Player player) const {
    std::array<int, 2> contribution = contributions(state);
    int pot = contribution[0] + contribution[1];

    ParsedHistory parsed = parse_history(state);
    const std::vector<Action>& final_round = active_round_actions(parsed);

    if (folded(final_round)) {
        Player folder = static_cast<Player>((final_round.size() - 1) % 2);
        Player winner = 1 - folder;
        return player == winner ? static_cast<double>(pot - contribution[winner])
                                 : -static_cast<double>(contribution[folder]);
    }

    return showdown_utility(state, player, contribution, pot);
}

InfoSetKey RhodeIslandGame::infoset_label(const State& state) const {
    Player player = current_player(state);
    ParsedHistory parsed = parse_history(state);

    std::string key = "P" + std::to_string(player) + ":" + card_name(state.private_cards[player]);
    for (std::uint8_t index = 0; index < state.public_count; ++index) {
        key += "|" + card_name(state.public_cards[index]);
    }
    key += ":";
    for (Action action : parsed.round1_actions) key += action_name(action) + ",";
    key += ";";
    for (Action action : parsed.round2_actions) key += action_name(action) + ",";
    key += ";";
    for (Action action : parsed.round3_actions) key += action_name(action) + ",";
    return key;
}

std::uint32_t RhodeIslandGame::infoset_count() const {
    return kRihRound3IndexBase + kRihRound3IndexSpan;
}

std::uint32_t RhodeIslandGame::infoset_index(const State& state) const {
    Player player = current_player(state);
    ParsedHistory parsed = parse_history(state);
    int private_card = state.private_cards[player];

    if (parsed.board_cards_dealt == 0) {
        int round1_progress = round_progress(parsed.round1_actions);
        return static_cast<std::uint32_t>(round1_progress * kRihDeckSize + private_card);
    }

    if (parsed.board_cards_dealt == 1) {
        int round1_ending = round_progress(parsed.round1_actions);
        int round2_progress = round_progress(parsed.round2_actions);
        int board_card0 = state.public_cards[0];
        std::uint32_t offset = ((static_cast<std::uint32_t>(round1_ending) * kRihRoundStageCount +
                                  static_cast<std::uint32_t>(round2_progress)) *
                                     kRihDeckSize +
                                 static_cast<std::uint32_t>(private_card)) *
                                    kRihDeckSize +
                                static_cast<std::uint32_t>(board_card0);
        return kRihRound2IndexBase + offset;
    }

    int round1_ending = round_progress(parsed.round1_actions);
    int round2_ending = round_progress(parsed.round2_actions);
    int round3_progress = round_progress(parsed.round3_actions);
    int board_card0 = state.public_cards[0];
    int board_card1 = state.public_cards[1];
    std::uint32_t offset = ((((static_cast<std::uint32_t>(round1_ending) * kRihRoundStageCount +
                                static_cast<std::uint32_t>(round2_ending)) *
                                   kRihRoundStageCount +
                               static_cast<std::uint32_t>(round3_progress)) *
                                  kRihDeckSize +
                              static_cast<std::uint32_t>(private_card)) *
                                 kRihDeckSize +
                             static_cast<std::uint32_t>(board_card0)) *
                                kRihDeckSize +
                            static_cast<std::uint32_t>(board_card1);
    return kRihRound3IndexBase + offset;
}

std::vector<std::pair<Action, double>> RhodeIslandGame::chance_outcomes(const State& state) const {
    std::vector<int> excluded;
    if (state.private_cards[0] != -1) excluded.push_back(state.private_cards[0]);
    if (state.private_cards[1] != -1) excluded.push_back(state.private_cards[1]);
    for (std::uint8_t index = 0; index < state.public_count; ++index) excluded.push_back(state.public_cards[index]);

    int remaining_count = kRihDeckSize - static_cast<int>(excluded.size());
    std::vector<std::pair<Action, double>> outcomes;
    for (int card = 0; card < kRihDeckSize; ++card) {
        if (std::find(excluded.begin(), excluded.end(), card) == excluded.end()) {
            outcomes.emplace_back(kRihChanceCardOffset + card, 1.0 / remaining_count);
        }
    }
    return outcomes;
}

}
