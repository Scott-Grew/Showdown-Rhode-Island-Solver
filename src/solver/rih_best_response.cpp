#include "solver/rih_best_response.hpp"

#include <algorithm>
#include <cstddef>
#include <numeric>

#include "eval/eval3.hpp"
#include "game/rhode_island.hpp"

namespace cfr::solver {

namespace {

using namespace cfr::game;

constexpr double kOrderedDealCount = 52.0 * 51.0 * 50.0 * 49.0;

struct BoardRanks {
    std::array<eval::HandRank, kCardCount> hand_rank{};
    std::array<int, kCardCount> cards_by_rank{};
};

BoardRanks rank_every_hole_card(int board0, int board1) {
    BoardRanks ranks;
    for (int card = 0; card < kCardCount; ++card) {
        std::array<Card, 3> hand = {static_cast<Card>(card), static_cast<Card>(board0), static_cast<Card>(board1)};
        ranks.hand_rank[card] = eval::evaluate_3card(hand.data());
        ranks.cards_by_rank[card] = card;
    }
    std::sort(ranks.cards_by_rank.begin(), ranks.cards_by_rank.end(),
              [&ranks](int left, int right) { return ranks.hand_rank[left] < ranks.hand_rank[right]; });
    return ranks;
}

class BestResponseWalk {
public:
    BestResponseWalk(const RihStrategyQuery& opponent_strategy, Player responder, bool maximize,
                      int deviation_round)
        : opponent_strategy_(opponent_strategy), responder_(responder), opponent_(1 - responder),
          maximize_(maximize), deviation_round_(deviation_round) {}

    RihCardVector walk(const State& state, const RihCardVector& opponent_reach, const BoardRanks* ranks) {
        if (game_.is_terminal(state)) return terminal_value(state, opponent_reach, ranks);
        if (game_.is_chance(state)) return deal_board_card(state, opponent_reach);
        if (game_.current_player(state) == responder_) return responder_node(state, opponent_reach, ranks);
        return opponent_node(state, opponent_reach, ranks);
    }

private:
    void clear_board_cards(const State& state, RihCardVector& value) const {
        for (std::uint8_t index = 0; index < state.public_count; ++index) {
            value[static_cast<std::size_t>(state.public_cards[index])] = 0.0;
        }
    }

    static double undealt_board_multiplicity(const State& state) {
        if (state.public_count == 0) return 50.0 * 49.0;
        if (state.public_count == 1) return 49.0;
        return 1.0;
    }

    RihCardVector terminal_value(const State& state, const RihCardVector& opponent_reach,
                                  const BoardRanks* ranks) const {
        ParsedRounds parsed = parse_rounds(state);
        std::span<const Action> final_round = active_round_actions(parsed);
        std::array<int, 2> contribution = rih_contributions(state);
        double board_multiplicity = undealt_board_multiplicity(state);
        RihCardVector value{};

        if (!final_round.empty() && final_round.back() == kActionFold) {
            Player folder = static_cast<Player>((final_round.size() - 1) % 2);
            int pot = contribution[0] + contribution[1];
            double payoff = responder_ == 1 - folder ? static_cast<double>(pot - contribution[1 - folder])
                                                      : -static_cast<double>(contribution[responder_]);
            double total_reach = std::accumulate(opponent_reach.begin(), opponent_reach.end(), 0.0);
            for (int card = 0; card < kCardCount; ++card) {
                value[static_cast<std::size_t>(card)] =
                    board_multiplicity * payoff *
                    (total_reach - opponent_reach[static_cast<std::size_t>(card)]);
            }
            clear_board_cards(state, value);
            return value;
        }

        double stake = static_cast<double>(contribution[responder_]);
        double weaker_reach = 0.0;
        std::size_t position = 0;
        double total_reach = std::accumulate(opponent_reach.begin(), opponent_reach.end(), 0.0);

        while (position < kCardCount) {
            std::size_t tie_end = position;
            eval::HandRank current = ranks->hand_rank[static_cast<std::size_t>(ranks->cards_by_rank[position])];
            double tie_reach = 0.0;
            while (tie_end < kCardCount &&
                   ranks->hand_rank[static_cast<std::size_t>(ranks->cards_by_rank[tie_end])] == current) {
                tie_reach += opponent_reach[static_cast<std::size_t>(ranks->cards_by_rank[tie_end])];
                ++tie_end;
            }
            for (std::size_t i = position; i < tie_end; ++i) {
                int card = ranks->cards_by_rank[i];
                double stronger_reach = total_reach - weaker_reach - tie_reach;
                value[static_cast<std::size_t>(card)] = stake * (weaker_reach - stronger_reach);
            }
            weaker_reach += tie_reach;
            position = tie_end;
        }

        clear_board_cards(state, value);
        return value;
    }

    RihCardVector deal_board_card(const State& state, const RihCardVector& opponent_reach) {
        RihCardVector value{};
        bool completes_board = state.public_count == 1;

        for (int board_card = 0; board_card < kCardCount; ++board_card) {
            bool already_public = false;
            for (std::uint8_t index = 0; index < state.public_count; ++index) {
                if (state.public_cards[index] == board_card) already_public = true;
            }
            if (already_public) continue;

            State child = game_.apply_action(state, kChanceCardOffset + board_card);
            RihCardVector child_reach = opponent_reach;
            child_reach[static_cast<std::size_t>(board_card)] = 0.0;

            BoardRanks completed_ranks;
            const BoardRanks* ranks = nullptr;
            if (completes_board) {
                completed_ranks = rank_every_hole_card(state.public_cards[0], board_card);
                ranks = &completed_ranks;
            }

            RihCardVector child_value = walk(child, child_reach, ranks);
            for (int card = 0; card < kCardCount; ++card) {
                if (card == board_card) continue;
                value[static_cast<std::size_t>(card)] += child_value[static_cast<std::size_t>(card)];
            }
        }
        return value;
    }

    RihCardVector responder_node(const State& state, const RihCardVector& opponent_reach, const BoardRanks* ranks) {
        std::vector<Action> actions = game_.legal_actions(state);
        RihCardVector value{};

        ParsedRounds parsed = parse_rounds(state);
        bool maximize_here = maximize_ && (deviation_round_ < 0 || parsed.board_cards_dealt == deviation_round_);

        std::vector<double> probabilities_by_card;
        if (!maximize_here) opponent_strategy_(state, responder_, probabilities_by_card);

        for (std::size_t action_index = 0; action_index < actions.size(); ++action_index) {
            RihCardVector child_value =
                walk(game_.apply_action(state, actions[action_index]), opponent_reach, ranks);
            for (int card = 0; card < kCardCount; ++card) {
                std::size_t slot = static_cast<std::size_t>(card);
                if (maximize_here) {
                    value[slot] = action_index == 0 ? child_value[slot] : std::max(value[slot], child_value[slot]);
                } else {
                    value[slot] += probabilities_by_card[slot * actions.size() + action_index] * child_value[slot];
                }
            }
        }
        return value;
    }

    RihCardVector opponent_node(const State& state, const RihCardVector& opponent_reach, const BoardRanks* ranks) {
        std::vector<Action> actions = game_.legal_actions(state);
        std::vector<double> probabilities_by_card;
        opponent_strategy_(state, opponent_, probabilities_by_card);

        RihCardVector value{};
        for (std::size_t action_index = 0; action_index < actions.size(); ++action_index) {
            RihCardVector child_reach{};
            for (int card = 0; card < kCardCount; ++card) {
                std::size_t slot = static_cast<std::size_t>(card);
                child_reach[slot] = opponent_reach[slot] * probabilities_by_card[slot * actions.size() + action_index];
            }
            RihCardVector child_value = walk(game_.apply_action(state, actions[action_index]), child_reach, ranks);
            for (int card = 0; card < kCardCount; ++card) {
                value[static_cast<std::size_t>(card)] += child_value[static_cast<std::size_t>(card)];
            }
        }
        return value;
    }

    RhodeIslandGame game_;
    const RihStrategyQuery& opponent_strategy_;
    Player responder_;
    Player opponent_;
    bool maximize_;
    int deviation_round_;
};

double walk_root(const RihStrategyQuery& strategy, Player responder, bool maximize, int deviation_round = -1) {
    RhodeIslandGame game;
    State root = game.initial_state();
    root = game.apply_action(root, kChanceCardOffset + 0);
    root = game.apply_action(root, kChanceCardOffset + 1);

    RihCardVector opponent_reach;
    opponent_reach.fill(1.0);

    BestResponseWalk walker(strategy, responder, maximize, deviation_round);
    RihCardVector value = walker.walk(root, opponent_reach, nullptr);
    return std::accumulate(value.begin(), value.end(), 0.0) / kOrderedDealCount;
}

}

double rih_best_response_value(const RihStrategyQuery& opponent_strategy, game::Player responder) {
    return walk_root(opponent_strategy, responder, true);
}

double rih_strategy_value(const RihStrategyQuery& profile, game::Player player) {
    return walk_root(profile, player, false);
}

double rih_best_response_value_in_round(const RihStrategyQuery& opponent_strategy, game::Player responder,
                                         int round) {
    return walk_root(opponent_strategy, responder, true, round);
}

double rih_exploitability(const RihStrategyQuery& profile) {
    return (rih_best_response_value(profile, 0) + rih_best_response_value(profile, 1)) / 2.0;
}

}
