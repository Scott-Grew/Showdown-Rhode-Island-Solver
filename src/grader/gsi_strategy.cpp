#include "grader/gsi_strategy.hpp"

#include <algorithm>
#include <fstream>
#include <utility>

#include "game/card.hpp"
#include "game/rhode_island.hpp"

namespace cfr::grader {

namespace {

constexpr int kGsiRound1KeySpan = kGsiRound1ClassCount * kGsiSlotsPerKey;
constexpr int kGsiRound2KeySpan = kGsiRound2ClassCount * kGsiSequencesPerRound * kGsiSlotsPerKey;

constexpr std::array<std::pair<int, int>, 4> kSlotRanges = {
    std::pair<int, int>{0, 2}, {2, 5}, {5, 8}, {8, 10}};

int gsi_card(Card card) {
    return card_suit(card) * kRankCount + card_rank(card);
}

int signal_index(int hole) {
    return 1 + hole;
}

int signal_index(int hole, int board0) {
    int shifted_board = board0 > hole ? board0 - 1 : board0;
    return kCardCount + kCardCount * shifted_board + signal_index(hole);
}

int signal_index(int hole, int board0, int board1) {
    int lower_cards = (hole < board1 ? 1 : 0) + (board0 < board1 ? 1 : 0);
    int shifted_board = board1 - lower_cards;
    return 2704 + 2652 * shifted_board + (signal_index(hole, board0) - kCardCount);
}

int gsi_sequence(const std::vector<game::Action>& round_actions) {
    int raises_used =
        static_cast<int>(std::count(round_actions.begin(), round_actions.end(), game::kActionRaise));
    bool opened_with_check = !round_actions.empty() && round_actions.front() == game::kActionCallCheck;
    return opened_with_check ? raises_used : raises_used + 3;
}

int decision_type(const std::vector<game::Action>& round_actions) {
    if (round_actions.empty() || round_actions.back() != game::kActionRaise) return 0;
    return static_cast<int>(std::count(round_actions.begin(), round_actions.end(), game::kActionRaise));
}

}

struct GsiStrategy::PublicContext {
    int board0 = -1;
    int board1 = -1;
    int sequence1 = 0;
    int sequence2 = 0;
    int board_cards_dealt = 0;
    int decision_type = 0;
    game::Player actor = 0;
};

GsiStrategy::PublicContext GsiStrategy::describe(const game::State& state) {
    game::ParsedRounds parsed = game::parse_rounds(state);
    const std::vector<game::Action>& round_actions = game::active_round_actions(parsed);

    PublicContext context;
    context.board_cards_dealt = parsed.board_cards_dealt;
    context.decision_type = decision_type(round_actions);
    context.actor = static_cast<game::Player>(round_actions.size() % 2);
    if (parsed.board_cards_dealt >= 1) {
        context.board0 = gsi_card(static_cast<Card>(state.public_cards[0]));
        context.sequence1 = gsi_sequence(parsed.round[0]);
    }
    if (parsed.board_cards_dealt >= 2) {
        context.board1 = gsi_card(static_cast<Card>(state.public_cards[1]));
        context.sequence2 = gsi_sequence(parsed.round[1]);
    }
    return context;
}

std::uint32_t GsiStrategy::key_for(const PublicContext& context, int hole) const {
    if (context.board_cards_dealt == 0) {
        int class_id = signal_class_[static_cast<std::size_t>(signal_index(hole))];
        return static_cast<std::uint32_t>(kGsiSlotsPerKey * (class_id - 1) + 1);
    }

    if (context.board_cards_dealt == 1) {
        int class_id = signal_class_[static_cast<std::size_t>(signal_index(hole, context.board0))];
        return static_cast<std::uint32_t>(
            kGsiSequencesPerRound * kGsiSlotsPerKey * (class_id - kGsiRound1ClassCount - 1) +
            kGsiSlotsPerKey * context.sequence1 + kGsiRound1KeySpan + 1);
    }

    int class_id = signal_class_[static_cast<std::size_t>(signal_index(hole, context.board0, context.board1))];
    return static_cast<std::uint32_t>(
        kGsiSequencesPerRound * kGsiSequencesPerRound * kGsiSlotsPerKey *
            (class_id - kGsiRound1ClassCount - kGsiRound2ClassCount - 1) +
        kGsiSlotsPerKey * (kGsiSequencesPerRound * context.sequence1 + context.sequence2) + kGsiRound1KeySpan +
        kGsiRound2KeySpan + 1);
}

void GsiStrategy::write_slots(game::Player actor, std::uint32_t key, int type, double* destination) const {
    auto [begin, end] = kSlotRanges[static_cast<std::size_t>(type)];
    std::size_t action_count = static_cast<std::size_t>(end - begin);

    double total = 0.0;
    auto entry = behavior_[static_cast<std::size_t>(actor)].find(key);
    if (entry != behavior_[static_cast<std::size_t>(actor)].end()) {
        for (int slot = begin; slot < end; ++slot) {
            double probability = std::max(0.0, static_cast<double>(entry->second[static_cast<std::size_t>(slot)]));
            destination[slot - begin] = probability;
            total += probability;
        }
    }

    if (total > 1e-9) {
        for (std::size_t i = 0; i < action_count; ++i) destination[i] /= total;
        return;
    }
    for (std::size_t i = 0; i < action_count; ++i) destination[i] = 1.0 / static_cast<double>(action_count);
}

GsiStrategy GsiStrategy::load(const std::string& directory) {
    GsiStrategy strategy;

    std::ifstream partition_file(directory + "/partition");
    if (!partition_file) return strategy;

    strategy.signal_class_.assign(kGsiSignalNodeCount + 1, 0);
    for (int hole = 0; hole < kCardCount; ++hole) {
        int class_id = 0;
        if (!(partition_file >> class_id)) return strategy;
        strategy.signal_class_[static_cast<std::size_t>(signal_index(hole))] = static_cast<std::uint16_t>(class_id);
        for (int board0 = 0; board0 < kCardCount; ++board0) {
            if (board0 == hole) continue;
            if (!(partition_file >> class_id)) return strategy;
            strategy.signal_class_[static_cast<std::size_t>(signal_index(hole, board0))] =
                static_cast<std::uint16_t>(class_id);
            for (int board1 = 0; board1 < kCardCount; ++board1) {
                if (board1 == hole || board1 == board0) continue;
                if (!(partition_file >> class_id)) return strategy;
                strategy.signal_class_[static_cast<std::size_t>(signal_index(hole, board0, board1))] =
                    static_cast<std::uint16_t>(class_id);
            }
        }
    }

    static const char* const kPlayerFiles[] = {"/player1", "/player2"};
    for (int player = 0; player < 2; ++player) {
        std::ifstream player_file(directory + kPlayerFiles[player]);
        if (!player_file) return strategy;
        std::uint32_t key = 0;
        while (player_file >> key) {
            std::array<float, kGsiSlotsPerKey> slots{};
            for (int slot = 0; slot < kGsiSlotsPerKey; ++slot) {
                if (!(player_file >> slots[static_cast<std::size_t>(slot)])) return strategy;
            }
            strategy.behavior_[static_cast<std::size_t>(player)].emplace(key, slots);
        }
    }

    strategy.available_ = true;
    return strategy;
}

std::vector<double> GsiStrategy::action_probabilities(const game::State& state) const {
    PublicContext context = describe(state);
    auto [begin, end] = kSlotRanges[static_cast<std::size_t>(context.decision_type)];
    std::vector<double> probabilities(static_cast<std::size_t>(end - begin), 0.0);
    int hole = gsi_card(static_cast<Card>(state.private_cards[context.actor]));
    write_slots(context.actor, key_for(context, hole), context.decision_type, probabilities.data());
    return probabilities;
}

void GsiStrategy::action_probabilities_by_card(const game::State& state, game::Player actor,
                                                std::vector<double>& probabilities_by_card) const {
    PublicContext context = describe(state);
    auto [begin, end] = kSlotRanges[static_cast<std::size_t>(context.decision_type)];
    std::size_t action_count = static_cast<std::size_t>(end - begin);
    probabilities_by_card.assign(static_cast<std::size_t>(kCardCount) * action_count, 0.0);

    for (int card = 0; card < kCardCount; ++card) {
        int hole = gsi_card(static_cast<Card>(card));
        if (hole == context.board0 || hole == context.board1) continue;
        write_slots(actor, key_for(context, hole), context.decision_type,
                    probabilities_by_card.data() + static_cast<std::size_t>(card) * action_count);
    }
}

}
