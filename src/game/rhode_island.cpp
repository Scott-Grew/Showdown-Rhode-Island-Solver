#include "game/rhode_island.hpp"

#include <algorithm>
#include <map>
#include <vector>
#include <array>
#include <cstddef>
#include <string>

#include "eval/eval3.hpp"
#include "game/card.hpp"
#include "game/game_concept.hpp"

namespace cfr::game {

static_assert(GameLike<RhodeIslandGame>);

RihParsedHistory rih_parse_history(const State& state) {
    RihParsedHistory parsed;
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

const std::vector<Action>& rih_active_round_actions(const RihParsedHistory& parsed) {
    if (parsed.board_cards_dealt == 0) return parsed.round1_actions;
    if (parsed.board_cards_dealt == 1) return parsed.round2_actions;
    return parsed.round3_actions;
}

namespace {

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

}

std::string rih_card_name(int card) {
    static const char* const kRankNames[] = {"2", "3", "4", "5", "6", "7", "8", "9", "T", "J", "Q", "K", "A"};
    static const char* const kSuitNames[] = {"c", "d", "h", "s"};
    Card typed_card = static_cast<Card>(card);
    return std::string(kRankNames[card_rank(typed_card)]) + kSuitNames[card_suit(typed_card)];
}

std::array<int, 2> rih_contributions(const State& state) {
    std::array<int, 2> contribution = {kRihAnte, kRihAnte};
    RihParsedHistory parsed = rih_parse_history(state);
    apply_round_contributions(parsed.round1_actions, kRihRound1Bet, contribution);
    if (parsed.board_cards_dealt >= 1) apply_round_contributions(parsed.round2_actions, kRihRound2Bet, contribution);
    if (parsed.board_cards_dealt >= 2) apply_round_contributions(parsed.round3_actions, kRihRound3Bet, contribution);
    return contribution;
}

namespace {

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

std::uint32_t relabelled(int card, std::array<int, kSuitCount>& suit_map, int& next_suit) {
    int suit = card_suit(static_cast<Card>(card));
    if (suit_map[static_cast<std::size_t>(suit)] < 0) suit_map[static_cast<std::size_t>(suit)] = next_suit++;
    return static_cast<std::uint32_t>(card_rank(static_cast<Card>(card)) * kSuitCount +
                                       suit_map[static_cast<std::size_t>(suit)]);
}

struct SuitCanonicalTables {
    std::vector<std::uint32_t> pair_class;
    std::vector<std::uint32_t> triple_class;
    std::uint32_t pair_count = 0;
    std::uint32_t triple_count = 0;
};

const SuitCanonicalTables& canonical_tables() {
    static const SuitCanonicalTables tables = [] {
        SuitCanonicalTables built;
        built.pair_class.assign(kRihDeckSize * kRihDeckSize, 0);
        built.triple_class.assign(kRihDeckSize * kRihDeckSize * kRihDeckSize, 0);

        std::map<std::uint32_t, std::uint32_t> pair_ids;
        std::map<std::uint32_t, std::uint32_t> triple_ids;

        for (int hole = 0; hole < kRihDeckSize; ++hole) {
            for (int board0 = 0; board0 < kRihDeckSize; ++board0) {
                if (board0 == hole) continue;
                std::array<int, kSuitCount> suit_map = {-1, -1, -1, -1};
                int next_suit = 0;
                std::uint32_t key = relabelled(hole, suit_map, next_suit) * kRihDeckSize +
                                     relabelled(board0, suit_map, next_suit);
                auto entry = pair_ids.find(key);
                if (entry == pair_ids.end()) entry = pair_ids.emplace(key, built.pair_count++).first;
                built.pair_class[static_cast<std::size_t>(hole * kRihDeckSize + board0)] = entry->second;

                for (int board1 = 0; board1 < kRihDeckSize; ++board1) {
                    if (board1 == hole || board1 == board0) continue;
                    std::array<int, kSuitCount> triple_suits = {-1, -1, -1, -1};
                    int next_triple_suit = 0;
                    std::uint32_t triple_key =
                        (relabelled(hole, triple_suits, next_triple_suit) * kRihDeckSize +
                         relabelled(board0, triple_suits, next_triple_suit)) *
                            kRihDeckSize +
                        relabelled(board1, triple_suits, next_triple_suit);
                    auto triple_entry = triple_ids.find(triple_key);
                    if (triple_entry == triple_ids.end()) {
                        triple_entry = triple_ids.emplace(triple_key, built.triple_count++).first;
                    }
                    built.triple_class[static_cast<std::size_t>((hole * kRihDeckSize + board0) * kRihDeckSize +
                                                                 board1)] = triple_entry->second;
                }
            }
        }
        return built;
    }();
    return tables;
}

}

State RhodeIslandGame::initial_state() const {
    return State{};
}

bool RhodeIslandGame::is_chance(const State& state) const {
    if (state.private_cards[0] == -1 || state.private_cards[1] == -1) return true;
    RihParsedHistory parsed = rih_parse_history(state);
    if (parsed.board_cards_dealt == 0) return round_closed(parsed.round1_actions);
    if (parsed.board_cards_dealt == 1) return round_closed(parsed.round2_actions);
    return false;
}

bool RhodeIslandGame::is_terminal(const State& state) const {
    if (is_chance(state)) return false;
    RihParsedHistory parsed = rih_parse_history(state);
    if (parsed.board_cards_dealt == 0) return folded(parsed.round1_actions);
    if (parsed.board_cards_dealt == 1) return folded(parsed.round2_actions);
    return folded(parsed.round3_actions) || round_closed(parsed.round3_actions);
}

Player RhodeIslandGame::current_player(const State& state) const {
    if (is_chance(state)) return -1;
    RihParsedHistory parsed = rih_parse_history(state);
    return round_actor(rih_active_round_actions(parsed));
}

std::vector<Action> RhodeIslandGame::legal_actions(const State& state) const {
    RihParsedHistory parsed = rih_parse_history(state);
    return round_legal_actions(rih_active_round_actions(parsed));
}

State RhodeIslandGame::apply_action(const State& state, Action action) const {
    State next = state;
    if (is_chance(state)) {
        next.history[next.history_len++] = static_cast<std::uint8_t>(action);
        int card = action - kRihChanceCardOffset;
        if (state.private_cards[0] == -1) {
            next.private_cards[0] = static_cast<std::int8_t>(card);
        } else if (state.private_cards[1] == -1) {
            next.private_cards[1] = static_cast<std::int8_t>(card);
        } else {
            next.public_cards[next.public_count++] = static_cast<std::int8_t>(card);
        }
        return next;
    }

    next.history[next.history_len++] = static_cast<std::uint8_t>(action);
    return next;
}

double RhodeIslandGame::terminal_utility(const State& state, Player player) const {
    std::array<int, 2> contribution = rih_contributions(state);
    int pot = contribution[0] + contribution[1];

    RihParsedHistory parsed = rih_parse_history(state);
    const std::vector<Action>& final_round = rih_active_round_actions(parsed);

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
    RihParsedHistory parsed = rih_parse_history(state);

    std::string key = "P" + std::to_string(player) + ":" + rih_card_name(state.private_cards[player]);
    for (std::uint8_t index = 0; index < state.public_count; ++index) {
        key += "|" + rih_card_name(state.public_cards[index]);
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
    const SuitCanonicalTables& tables = canonical_tables();
    return kRihRoundStageCount * kRankCount +
           kRihRoundStageCount * kRihRoundStageCount * tables.pair_count +
           kRihRoundStageCount * kRihRoundStageCount * kRihRoundStageCount * tables.triple_count;
}

std::uint32_t RhodeIslandGame::infoset_index(const State& state) const {
    Player player = current_player(state);
    RihParsedHistory parsed = rih_parse_history(state);
    const SuitCanonicalTables& tables = canonical_tables();
    std::uint32_t private_card = static_cast<std::uint32_t>(state.private_cards[player]);

    std::uint32_t round1_base = kRihRoundStageCount * kRankCount;
    std::uint32_t round2_base = round1_base + kRihRoundStageCount * kRihRoundStageCount * tables.pair_count;

    if (parsed.board_cards_dealt == 0) {
        std::uint32_t round1_progress = static_cast<std::uint32_t>(round_progress(parsed.round1_actions));
        return round1_progress * kRankCount + static_cast<std::uint32_t>(card_rank(static_cast<Card>(private_card)));
    }

    std::uint32_t board_card0 = static_cast<std::uint32_t>(state.public_cards[0]);
    std::uint32_t round1_ending = static_cast<std::uint32_t>(round_progress(parsed.round1_actions));

    if (parsed.board_cards_dealt == 1) {
        std::uint32_t round2_progress = static_cast<std::uint32_t>(round_progress(parsed.round2_actions));
        std::uint32_t pair_class = tables.pair_class[private_card * kRihDeckSize + board_card0];
        return round1_base + (round1_ending * kRihRoundStageCount + round2_progress) * tables.pair_count + pair_class;
    }

    std::uint32_t round2_ending = static_cast<std::uint32_t>(round_progress(parsed.round2_actions));
    std::uint32_t round3_progress = static_cast<std::uint32_t>(round_progress(parsed.round3_actions));
    std::uint32_t board_card1 = static_cast<std::uint32_t>(state.public_cards[1]);
    std::uint32_t triple_class =
        tables.triple_class[(private_card * kRihDeckSize + board_card0) * kRihDeckSize + board_card1];
    return round2_base +
           ((round1_ending * kRihRoundStageCount + round2_ending) * kRihRoundStageCount + round3_progress) *
               tables.triple_count +
           triple_class;
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
