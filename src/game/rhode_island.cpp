#include "game/rhode_island.hpp"

#include <map>
#include <vector>
#include <array>
#include <cstddef>
#include <string>

#include "eval/eval3.hpp"
#include "game/betting_round.hpp"
#include "game/card.hpp"
#include "game/game_concept.hpp"

namespace cfr::game {

static_assert(GameLike<RhodeIslandGame>);
static_assert(max_history_depth(3, kRihMaxRaisesPerRound, 4) <= static_cast<int>(kMaxHistory));

std::string rih_card_name(int card) {
    static const char* const kRankNames[] = {"2", "3", "4", "5", "6", "7", "8", "9", "T", "J", "Q", "K", "A"};
    static const char* const kSuitNames[] = {"c", "d", "h", "s"};
    Card typed_card = static_cast<Card>(card);
    return std::string(kRankNames[card_rank(typed_card)]) + kSuitNames[card_suit(typed_card)];
}

std::array<int, 2> rih_contributions(const State& state) {
    std::array<int, 2> contribution = {kRihAnte, kRihAnte};
    ParsedRounds parsed = parse_rounds(state);
    apply_round_contributions(parsed.round[0], kRihRound1Bet, contribution);
    if (parsed.board_cards_dealt >= 1) apply_round_contributions(parsed.round[1], kRihRound2Bet, contribution);
    if (parsed.board_cards_dealt >= 2) apply_round_contributions(parsed.round[2], kRihRound3Bet, contribution);
    return contribution;
}

namespace {

Player showdown_winner(const State& state) {
    std::array<Card, 3> hand0 = {static_cast<Card>(state.private_cards[0]), static_cast<Card>(state.public_cards[0]),
                                  static_cast<Card>(state.public_cards[1])};
    std::array<Card, 3> hand1 = {static_cast<Card>(state.private_cards[1]), static_cast<Card>(state.public_cards[0]),
                                  static_cast<Card>(state.public_cards[1])};
    cfr::eval::HandRank rank0 = cfr::eval::evaluate_3card(hand0.data());
    cfr::eval::HandRank rank1 = cfr::eval::evaluate_3card(hand1.data());
    if (rank0 == rank1) return -1;
    return rank0 > rank1 ? 0 : 1;
}

constexpr int kRihRoundStageCount = 8;

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
    ParsedRounds parsed = parse_rounds(state);
    if (parsed.board_cards_dealt >= 2) return false;
    return round_closed(active_round_actions(parsed));
}

bool RhodeIslandGame::is_terminal(const State& state) const {
    if (is_chance(state)) return false;
    ParsedRounds parsed = parse_rounds(state);
    const std::vector<Action>& active = active_round_actions(parsed);
    if (parsed.board_cards_dealt < 2) return folded(active);
    return folded(active) || round_closed(active);
}

Player RhodeIslandGame::current_player(const State& state) const {
    if (is_chance(state)) return -1;
    ParsedRounds parsed = parse_rounds(state);
    return round_actor(active_round_actions(parsed));
}

std::vector<Action> RhodeIslandGame::legal_actions(const State& state) const {
    ParsedRounds parsed = parse_rounds(state);
    return round_legal_actions(active_round_actions(parsed), kRihMaxRaisesPerRound);
}

State RhodeIslandGame::apply_action(const State& state, Action action) const {
    State next = state;
    bool dealing = is_chance(state);
    append_action(next, action);
    if (dealing) deal_card(next, action - kChanceCardOffset);
    return next;
}

double RhodeIslandGame::terminal_utility(const State& state, Player player) const {
    std::array<int, 2> contribution = rih_contributions(state);
    ParsedRounds parsed = parse_rounds(state);
    const std::vector<Action>& final_round = active_round_actions(parsed);

    if (folded(final_round)) {
        Player folder = static_cast<Player>((final_round.size() - 1) % 2);
        return settle(contribution, 1 - folder, player);
    }

    return settle(contribution, showdown_winner(state), player);
}

InfoSetKey RhodeIslandGame::infoset_label(const State& state) const {
    Player player = current_player(state);
    ParsedRounds parsed = parse_rounds(state);

    std::string key = "P" + std::to_string(player) + ":" + rih_card_name(state.private_cards[player]);
    for (std::uint8_t index = 0; index < state.public_count; ++index) {
        key += "|" + rih_card_name(state.public_cards[index]);
    }
    key += ":";
    for (Action action : parsed.round[0]) key += action_name(action) + ",";
    key += ";";
    for (Action action : parsed.round[1]) key += action_name(action) + ",";
    key += ";";
    for (Action action : parsed.round[2]) key += action_name(action) + ",";
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
    ParsedRounds parsed = parse_rounds(state);
    const SuitCanonicalTables& tables = canonical_tables();
    std::uint32_t private_card = static_cast<std::uint32_t>(state.private_cards[player]);

    std::uint32_t round1_base = kRihRoundStageCount * kRankCount;
    std::uint32_t round2_base = round1_base + kRihRoundStageCount * kRihRoundStageCount * tables.pair_count;

    if (parsed.board_cards_dealt == 0) {
        std::uint32_t round1_progress = static_cast<std::uint32_t>(round_progress(parsed.round[0]));
        return round1_progress * kRankCount + static_cast<std::uint32_t>(card_rank(static_cast<Card>(private_card)));
    }

    std::uint32_t board_card0 = static_cast<std::uint32_t>(state.public_cards[0]);
    std::uint32_t round1_ending = static_cast<std::uint32_t>(round_progress(parsed.round[0]));

    if (parsed.board_cards_dealt == 1) {
        std::uint32_t round2_progress = static_cast<std::uint32_t>(round_progress(parsed.round[1]));
        std::uint32_t pair_class = tables.pair_class[private_card * kRihDeckSize + board_card0];
        return round1_base + (round1_ending * kRihRoundStageCount + round2_progress) * tables.pair_count + pair_class;
    }

    std::uint32_t round2_ending = static_cast<std::uint32_t>(round_progress(parsed.round[1]));
    std::uint32_t round3_progress = static_cast<std::uint32_t>(round_progress(parsed.round[2]));
    std::uint32_t board_card1 = static_cast<std::uint32_t>(state.public_cards[1]);
    std::uint32_t triple_class =
        tables.triple_class[(private_card * kRihDeckSize + board_card0) * kRihDeckSize + board_card1];
    return round2_base +
           ((round1_ending * kRihRoundStageCount + round2_ending) * kRihRoundStageCount + round3_progress) *
               tables.triple_count +
           triple_class;
}

std::vector<std::pair<Action, double>> RhodeIslandGame::chance_outcomes(const State& state) const {
    return deal_outcomes(state, kRihDeckSize);
}

}
