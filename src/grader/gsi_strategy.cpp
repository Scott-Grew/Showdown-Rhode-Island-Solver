// Parsing of the GSI.jar strategy files and the arithmetic that turns
// a game state into a row key in them.

#include "grader/gsi_strategy.hpp"

#include <algorithm>
#include <cassert>
#include <fstream>
#include <stdexcept>

#include "game/card.hpp"
#include "game/rhode_island.hpp"

namespace cfr::grader {

namespace {

// Key ranges taken by rounds 1 and 2; later rounds start above.
constexpr int kGsiRound1KeySpan = kGsiRound1ClassCount * kGsiSlotsPerKey;
constexpr int kGsiRound2KeySpan =
    kGsiRound2ClassCount * kGsiSequencesPerRound * kGsiSlotsPerKey;

// Ordered (hole, board) pairs, the node count of signal-tree
// level 2; level 3 starts after the root's 52 and these.
constexpr int kHoleBoardPairCount = kCardCount * (kCardCount - 1);
constexpr int kSignalLevel3Base = kCardCount + kHoleBoardPairCount;
static_assert(kSignalLevel3Base + kHoleBoardPairCount * (kCardCount - 2) ==
              kGsiSignalNodeCount);

// A row whose slots sum to this or less is treated as absent.
constexpr double kEmptyRowTotal = 1e-9;

// One row of a player file is a key followed by ten slots:
//
//   slots   | 0 1       | 2 3 4       | 5 6 7       | 8 9
//   facing  | no bet    | first raise | second      | third
//   actions | check bet | fold call   | fold call   | fold call
//           |           | raise       | raise       |
//
// Slot range of a row per decision type: no bet faced, then
// facing the first, second or third raise.
struct SlotRange {
    int begin;
    int end;
};
constexpr std::array<SlotRange, 4> kSlotRanges = {
    SlotRange{0, 2}, {2, 5}, {5, 8}, {8, 10}};

// GSI numbers cards suit-major, suit * 13 + rank; ours are rank-major.
// The ace of suit 0 is 12 in GSI and 48 here.
int gsi_card(Card card) {
    return card_suit(card) * kRankCount + card_rank(card);
}

// Signal-tree node of a lone hole card; node 0 is the root.
int signal_index(int hole) {
    return 1 + hole;
}

// Node of a hole card plus first board card. The board index skips
// the hole card, leaving 51 values.
int signal_index(int hole, int board0) {
    int shifted_board = board0 > hole ? board0 - 1 : board0;
    return kCardCount + kCardCount * shifted_board + signal_index(hole);
}

// Node of a full deal. The second board index skips both earlier
// cards, leaving 50 values.
int signal_index(int hole, int board0, int board1) {
    int lower_cards = (hole < board1 ? 1 : 0) + (board0 < board1 ? 1 : 0);
    int shifted_board = board1 - lower_cards;
    return kSignalLevel3Base + kHoleBoardPairCount * shifted_board +
           (signal_index(hole, board0) - kCardCount);
}

}

// Board cards are GSI card ids and betting is GSI sequence ids;
// -1 marks a board card that is not dealt yet.
struct GsiStrategy::PublicContext {
    int board0 = -1;
    int board1 = -1;
    int sequence1 = 0;
    int sequence2 = 0;
    int board_cards_dealt = 0;
    int decision_type = 0;
    game::Player acting_player = 0;
};

// Sequence ids are the closing codes stored when each board card
// was dealt. decision_type is 0 with no bet faced, else the raise count.
GsiStrategy::PublicContext GsiStrategy::public_context(
    const game::State& state) {
    PublicContext context;
    context.board_cards_dealt = state.board_count;
    bool facing_bet =
        state.round_len != 0 && game::last_action(state) == game::kActionRaise;
    context.decision_type =
        facing_bet ? static_cast<int>(state.raises_used) : 0;
    context.acting_player = state.round_len % game::kPlayerCount;
    if (state.board_count >= 1) {
        context.board0 = gsi_card(static_cast<Card>(state.board_cards[0]));
        context.sequence1 = state.closing[0];
    }
    if (state.board_count >= 2) {
        context.board1 = gsi_card(static_cast<Card>(state.board_cards[1]));
        context.sequence2 = state.closing[1];
    }
    return context;
}

// Row key in a player file. Keys are 1-based, classes are numbered
// across rounds, and each round's keys follow the previous one's.
std::uint32_t GsiStrategy::key_for(const PublicContext& context,
                                   int hole) const {
    if (context.board_cards_dealt == 0) {
        int class_id =
            signal_class_[static_cast<std::size_t>(signal_index(hole))];
        return static_cast<std::uint32_t>(kGsiSlotsPerKey * (class_id - 1) + 1);
    }

    if (context.board_cards_dealt == 1) {
        int class_id = signal_class_[static_cast<std::size_t>(
            signal_index(hole, context.board0))];
        return static_cast<std::uint32_t>(
            kGsiSequencesPerRound * kGsiSlotsPerKey *
                (class_id - kGsiRound1ClassCount - 1) +
            kGsiSlotsPerKey * context.sequence1 + kGsiRound1KeySpan + 1);
    }

    int class_id = signal_class_[static_cast<std::size_t>(
        signal_index(hole, context.board0, context.board1))];
    return static_cast<std::uint32_t>(
        kGsiSequencesPerRound * kGsiSequencesPerRound * kGsiSlotsPerKey *
            (class_id - kGsiRound1ClassCount - kGsiRound2ClassCount - 1) +
        kGsiSlotsPerKey *
            (kGsiSequencesPerRound * context.sequence1 + context.sequence2) +
        kGsiRound1KeySpan + kGsiRound2KeySpan + 1);
}

// Negative slots clamp to zero before normalizing; a row at or
// below kEmptyRowTotal becomes uniform.
void GsiStrategy::write_slots(game::Player acting_player, std::uint32_t key,
                              int decision_type,
                              std::span<double> destination) const {
    auto [begin, end] = kSlotRanges[static_cast<std::size_t>(decision_type)];
    std::size_t action_count = static_cast<std::size_t>(end - begin);

    double total = 0.0;
    auto entry =
        strategy_rows_[static_cast<std::size_t>(acting_player)].find(key);
    if (entry !=
        strategy_rows_[static_cast<std::size_t>(acting_player)].end()) {
        for (int slot = begin; slot < end; ++slot) {
            double probability = std::max(
                0.0, static_cast<double>(
                         entry->second[static_cast<std::size_t>(slot)]));
            destination[static_cast<std::size_t>(slot - begin)] = probability;
            total += probability;
        }
    }

    if (total > kEmptyRowTotal) {
        for (std::size_t i = 0; i < action_count; ++i) destination[i] /= total;
        return;
    }
    for (std::size_t i = 0; i < action_count; ++i)
        destination[i] = 1.0 / static_cast<double>(action_count);
}

// Reads the partition in hole, board, board nesting order, then
// both player files of key plus ten floats per row.
GsiStrategy GsiStrategy::load(const std::string& directory) {
    GsiStrategy strategy;

    const std::string partition_path = directory + "/partition";
    std::ifstream partition_file(partition_path);
    if (!partition_file) {
        throw std::runtime_error("gsi: could not open '" + partition_path +
                                 "'; run `make data/gsi/partition`");
    }

    auto next_class = [&] {
        int class_id = 0;
        if (!(partition_file >> class_id)) {
            throw std::runtime_error("gsi: truncated partition file '" +
                                     partition_path + "'");
        }
        return static_cast<std::uint16_t>(class_id);
    };
    strategy.signal_class_.assign(kGsiSignalNodeCount + 1, 0);
    for (int hole = 0; hole < kCardCount; ++hole) {
        strategy.signal_class_[static_cast<std::size_t>(signal_index(hole))] =
            next_class();
        for (int board0 = 0; board0 < kCardCount; ++board0) {
            if (board0 == hole) continue;
            strategy.signal_class_[static_cast<std::size_t>(
                signal_index(hole, board0))] = next_class();
            for (int board1 = 0; board1 < kCardCount; ++board1) {
                if (board1 == hole || board1 == board0) continue;
                strategy.signal_class_[static_cast<std::size_t>(
                    signal_index(hole, board0, board1))] = next_class();
            }
        }
    }

    static const char* const kPlayerFiles[] = {"/player1", "/player2"};
    for (int player = 0; player < 2; ++player) {
        const std::string player_path = directory + kPlayerFiles[player];
        std::ifstream player_file(player_path);
        if (!player_file) {
            throw std::runtime_error("gsi: could not open '" + player_path +
                                     "'; run `make data/gsi/partition`");
        }
        std::uint32_t key = 0;
        while (player_file >> key) {
            std::array<float, kGsiSlotsPerKey> slots{};
            for (int slot = 0; slot < kGsiSlotsPerKey; ++slot) {
                if (!(player_file >> slots[static_cast<std::size_t>(slot)])) {
                    throw std::runtime_error(
                        "gsi: truncated or malformed row in '" + player_path +
                        "' at key " + std::to_string(key));
                }
            }
            strategy.strategy_rows_[static_cast<std::size_t>(player)].emplace(
                key, slots);
        }
        if (!player_file.eof()) {
            throw std::runtime_error("gsi: malformed row in '" + player_path +
                                     "'");
        }
    }

    return strategy;
}

// Looks up the row of the acting player's own hole card.
std::vector<double> GsiStrategy::action_probabilities(
    const game::State& state) const {
    PublicContext context = public_context(state);
    auto [begin, end] =
        kSlotRanges[static_cast<std::size_t>(context.decision_type)];
    std::vector<double> probabilities(static_cast<std::size_t>(end - begin),
                                      0.0);
    int hole =
        gsi_card(static_cast<Card>(state.hole_cards[context.acting_player]));
    write_slots(context.acting_player, key_for(context, hole),
                context.decision_type, probabilities);
    return probabilities;
}

std::uint16_t GsiStrategy::signal_class(std::span<const Card> own_cards) const {
    assert(own_cards.size() >= 1 && own_cards.size() <= 3);
    int hole = gsi_card(own_cards[0]);
    if (own_cards.size() == 1) {
        return signal_class_[static_cast<std::size_t>(signal_index(hole))];
    }

    int board0 = gsi_card(own_cards[1]);
    if (own_cards.size() == 2) {
        return signal_class_[static_cast<std::size_t>(
            signal_index(hole, board0))];
    }

    int board1 = gsi_card(own_cards[2]);
    return signal_class_[static_cast<std::size_t>(
        signal_index(hole, board0, board1))];
}

// Skips hole cards that are on the board; their entries stay
// zero.
void GsiStrategy::action_probabilities_by_card(
    const game::State& state, game::Player acting_player,
    std::vector<double>& probabilities_by_card) const {
    PublicContext context = public_context(state);
    auto [begin, end] =
        kSlotRanges[static_cast<std::size_t>(context.decision_type)];
    std::size_t action_count = static_cast<std::size_t>(end - begin);
    probabilities_by_card.assign(
        static_cast<std::size_t>(kCardCount) * action_count, 0.0);

    for (int card = 0; card < kCardCount; ++card) {
        int hole = gsi_card(static_cast<Card>(card));
        if (hole == context.board0 || hole == context.board1) continue;
        std::span<double> card_probabilities(
            probabilities_by_card.data() +
                static_cast<std::size_t>(card) * action_count,
            action_count);
        write_slots(acting_player, key_for(context, hole),
                    context.decision_type, card_probabilities);
    }
}

}
