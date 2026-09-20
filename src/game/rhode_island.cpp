// Rhode Island rules built on betting_round.hpp, showdowns from
// eval3, and the suit-canonical infoset index, which gives one table
// row to hands that differ only by a relabelling of suits.

#include "game/rhode_island.hpp"

#include <initializer_list>
#include <map>
#include <vector>
#include <array>
#include <cstddef>
#include <string>

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

// Rank character followed by suit character.
std::string rih_card_name(int card) {
    static const char* const kRankNames[] = {"2", "3", "4", "5", "6", "7", "8",
                                             "9", "T", "J", "Q", "K", "A"};
    static const char* const kSuitNames[] = {"c", "d", "h", "s"};
    Card typed_card = static_cast<Card>(card);
    return std::string(kRankNames[card_rank(typed_card)]) +
           kSuitNames[card_suit(typed_card)];
}

// Ante plus the bets of each round played so far.
std::array<int, 2> rih_contributions(const State& state) {
    std::array<int, 2> contributions = {kRihAnte, kRihAnte};
    ParsedRounds parsed = parse_rounds(state);
    apply_round_contributions(parsed.round(0), kRihRound1Bet, contributions);
    if (parsed.board_cards_dealt >= 1)
        apply_round_contributions(parsed.round(1), kRihRound2Bet,
                                  contributions);
    if (parsed.board_cards_dealt >= 2)
        apply_round_contributions(parsed.round(2), kRihRound3Bet,
                                  contributions);
    return contributions;
}

namespace {

// Winner by three-card hand rank over hole card plus board, or -1
// on a tie.
Player showdown_winner(const State& state) {
    std::array<Card, 3> hand0 = {static_cast<Card>(state.hole_cards[0]),
                                 static_cast<Card>(state.board_cards[0]),
                                 static_cast<Card>(state.board_cards[1])};
    std::array<Card, 3> hand1 = {static_cast<Card>(state.hole_cards[1]),
                                 static_cast<Card>(state.board_cards[0]),
                                 static_cast<Card>(state.board_cards[1])};
    cfr::eval::HandRank rank0 = cfr::eval::evaluate_3card(hand0.data());
    cfr::eval::HandRank rank1 = cfr::eval::evaluate_3card(hand1.data());
    if (rank0 == rank1) return kNoWinner;
    return rank0 > rank1 ? 0 : 1;
}

// Number of round_progress values in one round.
constexpr int kRihRoundProgressCount =
    round_progress_count(kRihMaxRaisesPerRound);

// Card id with its suit renamed to the order suits first appear in
// the hand, so hands equal up to a suit swap get equal keys.
std::uint32_t relabelled(int card, std::array<int, kSuitCount>& suit_map,
                         int& next_suit) {
    int suit = card_suit(static_cast<Card>(card));
    if (suit_map[static_cast<std::size_t>(suit)] < 0)
        suit_map[static_cast<std::size_t>(suit)] = next_suit++;
    return static_cast<std::uint32_t>(card_rank(static_cast<Card>(card)) *
                                          kSuitCount +
                                      suit_map[static_cast<std::size_t>(suit)]);
}

// Suit-isomorphism class of every ordered (hole, board) pair and
// (hole, board, board) triple, indexed by raw card ids.
struct SuitCanonicalTables {
    std::vector<std::uint32_t> pair_class;
    std::vector<std::uint32_t> triple_class;
    std::uint32_t pair_count = 0;
    std::uint32_t triple_count = 0;
};

// Code of a card sequence after its suits are renamed in order of
// first appearance: the relabelled card ids as base-52 digits.
std::uint32_t suit_canonical_code(std::initializer_list<int> cards) {
    std::array<int, kSuitCount> suit_map = {-1, -1, -1, -1};
    int next_suit = 0;
    std::uint32_t code = 0;
    for (int card : cards) {
        code = code * kCardCount + relabelled(card, suit_map, next_suit);
    }
    return code;
}

// Class id of a code; new codes are numbered in order of arrival.
std::uint32_t class_of(std::map<std::uint32_t, std::uint32_t>& class_ids,
                       std::uint32_t code, std::uint32_t& class_count) {
    auto entry = class_ids.find(code);
    if (entry == class_ids.end()) {
        entry = class_ids.emplace(code, class_count++).first;
    }
    return entry->second;
}

// Fills both class tables by visiting every ordered deal.
SuitCanonicalTables build_canonical_tables() {
    SuitCanonicalTables built;
    built.pair_class.assign(kCardCount * kCardCount, 0);
    built.triple_class.assign(kCardCount * kCardCount * kCardCount, 0);
    std::map<std::uint32_t, std::uint32_t> pair_ids;
    std::map<std::uint32_t, std::uint32_t> triple_ids;

    for (int hole = 0; hole < kCardCount; ++hole) {
        for (int board0 = 0; board0 < kCardCount; ++board0) {
            if (board0 == hole) continue;
            std::size_t pair_index =
                static_cast<std::size_t>(hole * kCardCount + board0);
            built.pair_class[pair_index] =
                class_of(pair_ids, suit_canonical_code({hole, board0}),
                         built.pair_count);

            for (int board1 = 0; board1 < kCardCount; ++board1) {
                if (board1 == hole || board1 == board0) continue;
                std::uint32_t triple_code =
                    suit_canonical_code({hole, board0, board1});
                built.triple_class[pair_index * kCardCount +
                                   static_cast<std::size_t>(board1)] =
                    class_of(triple_ids, triple_code, built.triple_count);
            }
        }
    }
    return built;
}

// The class tables, built on first use; a function-local static,
// so the build is thread-safe.
const SuitCanonicalTables& canonical_tables() {
    static const SuitCanonicalTables tables = build_canonical_tables();
    return tables;
}

}

// A default State: no cards and an empty history.
State RhodeIslandGame::initial_state() const {
    return State{};
}

// True while a hole card is missing, or when a round has closed
// and fewer than two board cards are out.
bool RhodeIslandGame::is_chance(const State& state) const {
    if (state.hole_cards[0] == -1 || state.hole_cards[1] == -1) return true;
    ParsedRounds parsed = parse_rounds(state);
    if (parsed.board_cards_dealt >= 2) return false;
    return round_closed(active_round_actions(parsed));
}

// A fold ends the hand in any round; otherwise it ends when
// round 3 closes.
bool RhodeIslandGame::is_terminal(const State& state) const {
    if (is_chance(state)) return false;
    ParsedRounds parsed = parse_rounds(state);
    std::span<const Action> active = active_round_actions(parsed);
    if (parsed.board_cards_dealt < 2) return folded(active);
    return folded(active) || round_closed(active);
}

// Player to act in the active round, or -1 at a chance node.
Player RhodeIslandGame::current_player(const State& state) const {
    if (is_chance(state)) return kChancePlayer;
    ParsedRounds parsed = parse_rounds(state);
    return round_acting_player(active_round_actions(parsed));
}

// Legal actions of the active round under the raise cap.
std::vector<Action> RhodeIslandGame::legal_actions(const State& state) const {
    ParsedRounds parsed = parse_rounds(state);
    return round_legal_actions(active_round_actions(parsed),
                               kRihMaxRaisesPerRound);
}

// Appends the entry, and at a chance node also stores the card
// decoded from action - kChanceCardOffset.
State RhodeIslandGame::apply_action(const State& state, Action action) const {
    State next = state;
    bool dealing = is_chance(state);
    append_action(next, action);
    if (dealing) deal_card(next, action - kChanceCardOffset);
    return next;
}

// A fold pays the other player; otherwise the showdown
// decides, and a tie pays zero.
double RhodeIslandGame::terminal_utility(const State& state,
                                         Player player) const {
    std::array<int, 2> contributions = rih_contributions(state);
    ParsedRounds parsed = parse_rounds(state);
    std::span<const Action> final_round = active_round_actions(parsed);

    if (folded(final_round)) {
        Player folder = static_cast<Player>((final_round.size() - 1) % 2);
        return settle(contributions, 1 - folder, player);
    }

    return settle(contributions, showdown_winner(state), player);
}

// Builds "P<player>:<hole>|<board>:<r1>;<r2>;<r3>" with full card
// names, so labels are finer than the suit-canonical index.
InfosetLabel RhodeIslandGame::infoset_label(const State& state) const {
    Player acting_player = current_player(state);
    ParsedRounds parsed = parse_rounds(state);

    std::string label = "P" + std::to_string(acting_player) + ":" +
                        rih_card_name(state.hole_cards[acting_player]);
    for (std::uint8_t index = 0; index < state.board_count; ++index) {
        label += "|" + rih_card_name(state.board_cards[index]);
    }
    label += ":";
    for (Action action : parsed.round(0)) label += action_name(action) + ",";
    label += ";";
    for (Action action : parsed.round(1)) label += action_name(action) + ",";
    label += ";";
    for (Action action : parsed.round(2)) label += action_name(action) + ",";
    return label;
}

// Rows of all three rounds; infoset_index has the layout.
std::uint32_t RhodeIslandGame::infoset_count() const {
    const SuitCanonicalTables& tables = canonical_tables();
    return kRihRoundProgressCount * kRankCount +
           kRihRoundProgressCount * kRihRoundProgressCount * tables.pair_count +
           kRihRoundProgressCount * kRihRoundProgressCount *
               kRihRoundProgressCount * tables.triple_count;
}

// round_progress of one round as an unsigned table coordinate.
static std::uint32_t progress_code(std::span<const Action> round_actions) {
    return static_cast<std::uint32_t>(round_progress(round_actions));
}

// Row layout of the infoset table, with P = kRihRoundProgressCount:
//
//   round 1 | P * 13 rows
//           |   keyed by progress and hole rank
//   round 2 | P^2 * pair_count rows
//           |   keyed by progress of rounds 1-2, class of 2 cards
//   round 3 | P^3 * triple_count rows
//           |   keyed by progress of rounds 1-3, class of all 3 cards
//
// infoset_count returns the total, 5,139,368 rows.
//
// Round 1 rows key on progress and rank. Later rounds key on the
// progress of every round so far and the suit class of the cards.
std::uint32_t RhodeIslandGame::infoset_index(const State& state) const {
    Player acting_player = current_player(state);
    ParsedRounds parsed = parse_rounds(state);
    std::uint32_t hole_card =
        static_cast<std::uint32_t>(state.hole_cards[acting_player]);
    std::uint32_t round1_progress = progress_code(parsed.round(0));

    if (parsed.board_cards_dealt == 0) {
        std::uint32_t hole_rank =
            static_cast<std::uint32_t>(card_rank(static_cast<Card>(hole_card)));
        return round1_progress * kRankCount + hole_rank;
    }

    const SuitCanonicalTables& tables = canonical_tables();
    std::uint32_t round1_rows = kRihRoundProgressCount * kRankCount;
    std::uint32_t board0 = static_cast<std::uint32_t>(state.board_cards[0]);
    std::uint32_t pair_index = hole_card * kCardCount + board0;
    std::uint32_t round2_betting = round1_progress * kRihRoundProgressCount +
                                   progress_code(parsed.round(1));

    if (parsed.board_cards_dealt == 1) {
        return round1_rows + round2_betting * tables.pair_count +
               tables.pair_class[pair_index];
    }

    std::uint32_t round2_rows =
        kRihRoundProgressCount * kRihRoundProgressCount * tables.pair_count;
    std::uint32_t board1 = static_cast<std::uint32_t>(state.board_cards[1]);
    std::uint32_t triple_index = pair_index * kCardCount + board1;
    std::uint32_t round3_betting = round2_betting * kRihRoundProgressCount +
                                   progress_code(parsed.round(2));
    return round1_rows + round2_rows + round3_betting * tables.triple_count +
           tables.triple_class[triple_index];
}

// Uniform over the undealt cards of the deck.
std::vector<ChanceOutcome> RhodeIslandGame::chance_outcomes(
    const State& state) const {
    return deal_outcomes(state, kCardCount);
}

}
