// The cfr_play program: a terminal game of Rhode Island hold'em
// against a trained checkpoint or the published equilibrium.

#include <algorithm>
#include <functional>
#include <iostream>
#include <numeric>
#include <optional>
#include <random>
#include <string>
#include <vector>

#include "game/rhode_island.hpp"
#include "grader/gsi_strategy.hpp"
#include "solver/mccfr_solver.hpp"

using namespace cfr::game;

namespace {

// Seats: the human acts first in every round.
constexpr Player kHuman = 0;
constexpr Player kSolver = 1;

// Word shown for an action: check and bet when no bet is faced,
// call and raise otherwise.
std::string action_prompt(Action action, const State& state) {
    ParsedRounds parsed = parse_rounds(state);
    std::span<const Action> round_actions = active_round_actions(parsed);
    bool facing_bet =
        !round_actions.empty() && round_actions.back() == kActionRaise;
    switch (action) {
        case kActionFold: return "fold";
        case kActionCallCheck: return facing_bet ? "call" : "check";
        case kActionRaise: return facing_bet ? "raise" : "bet";
    }
    return "?";
}

// Prints the human's card, the board and the pot, plus the
// solver's card when asked.
void show_table(const State& state, bool reveal_solver_hole) {
    std::cout << "\n  your card " << rih_card_name(state.hole_cards[kHuman]);
    if (state.board_count > 0) {
        std::cout << "   board";
        for (std::uint8_t i = 0; i < state.board_count; ++i) {
            std::cout << " " << rih_card_name(state.board_cards[i]);
        }
    }
    if (reveal_solver_hole)
        std::cout << "   solver card "
                  << rih_card_name(state.hole_cards[kSolver]);
    std::array<int, 2> contributions = rih_contributions(state);
    std::cout << "   pot " << contributions[0] + contributions[1] << "\n";
}

// Reads a legal move by name from stdin; end of input returns a
// fold and leaves std::cin failed.
Action ask_human(const RhodeIslandGame& game, const State& state) {
    std::vector<Action> actions = game.legal_actions(state);
    while (true) {
        std::cout << "  your move [";
        for (std::size_t i = 0; i < actions.size(); ++i) {
            std::cout << (i ? "/" : "") << action_prompt(actions[i], state);
        }
        std::cout << "]: " << std::flush;

        std::string typed;
        if (!(std::cin >> typed)) return kActionFold;
        std::transform(
            typed.begin(), typed.end(), typed.begin(),
            [](unsigned char character) { return std::tolower(character); });
        for (Action action : actions) {
            if (action_prompt(action, state) == typed) return action;
        }
        std::cout << "  not a legal move here\n";
    }
}

// Seed for a solver that only loads a checkpoint and never samples.
constexpr std::uint64_t kCheckpointLoaderSeed = 1;

// Strategy lookup: state and action count to probabilities.
using ActionProbabilities =
    std::function<std::vector<double>(const State&, std::size_t)>;

// Samples the solver's action from its strategy at state.
Action solver_move(const RhodeIslandGame& game, const State& state,
                   const ActionProbabilities& strategy,
                   std::mt19937_64& random_engine) {
    std::vector<Action> actions = game.legal_actions(state);
    std::vector<double> probabilities = strategy(state, actions.size());
    double roll =
        std::uniform_real_distribution<double>(0.0, 1.0)(random_engine);
    double cumulative = 0.0;
    for (std::size_t i = 0; i + 1 < actions.size(); ++i) {
        cumulative += probabilities[i];
        if (roll < cumulative) return actions[i];
    }
    return actions.back();
}

// Deals and plays one hand from a shuffled deck. Returns the chips
// the human won, or nothing when input ended mid-hand.
std::optional<double> play_hand(const RhodeIslandGame& game,
                                const ActionProbabilities& strategy,
                                const std::vector<int>& deck,
                                std::mt19937_64& random_engine) {
    State state = game.initial_state();
    state = game.apply_action(state, kChanceCardOffset + deck[0]);
    state = game.apply_action(state, kChanceCardOffset + deck[1]);
    int next_board_card = kHoleCardsDealt;

    while (!game.is_terminal(state)) {
        if (game.is_chance(state)) {
            state = game.apply_action(
                state, kChanceCardOffset + deck[next_board_card++]);
            show_table(state, false);
            continue;
        }
        if (game.current_player(state) == kHuman) {
            bool first_decision =
                state.history_len == kHoleCardsDealt && state.board_count == 0;
            if (first_decision) show_table(state, false);
            Action chosen = ask_human(game, state);
            if (!std::cin) return std::nullopt;
            state = game.apply_action(state, chosen);
            continue;
        }
        Action chosen = solver_move(game, state, strategy, random_engine);
        std::cout << "  solver " << action_prompt(chosen, state) << "\n";
        state = game.apply_action(state, chosen);
    }

    show_table(state, true);
    return game.terminal_utility(state, kHuman);
}

// Prints the hand's outcome and the running total in chips.
void report_result(double human_result, long long hands_played,
                   double human_total) {
    std::string outcome = "push";
    if (human_result != 0) {
        long long chips = static_cast<long long>(std::abs(human_result));
        outcome = (human_result > 0 ? "win " : "lose ") + std::to_string(chips);
    }
    std::cout << "  you " << outcome << "\n  after " << hands_played
              << " hands you are " << (human_total >= 0 ? "+" : "")
              << static_cast<long long>(human_total) << " ("
              << human_total / static_cast<double>(hands_played)
              << " per hand)\n";
}

// Tells the user how to fetch the published strategy files.
void print_missing_data_help() {
    std::cerr << "no equilibrium data in " << CFR_GSI_DATA_DIR << "\n"
              << "pass a solved strategy file instead, or fetch theirs "
                 "with:\n"
              << "  curl -O http://www.cs.cmu.edu/~gilpin/GSI.jar\n"
              << "  unzip -j GSI.jar 'strategy/*' -d data/gsi\n";
}

}

// Plays hands against a checkpoint given as argv[1], or against
// the published equilibrium when no argument is given.
int main(int argc, char** argv) {
    RhodeIslandGame game;
    ActionProbabilities strategy;
    std::string opponent_name;

    cfr::solver::ExternalSamplingSolver<RhodeIslandGame> solved(
        game, kCheckpointLoaderSeed);
    cfr::grader::GsiStrategy published;

    if (argc > 1) {
        solved.load_strategy_sums(argv[1]);
        opponent_name = "our solver, " +
                        std::to_string(solved.iterations_run()) + " iterations";
        strategy = [&game, &solved](const State& state,
                                    std::size_t action_count) {
            std::vector<double> probabilities(action_count);
            solved.average_strategy_into(game.infoset_index(state),
                                         action_count, probabilities);
            return probabilities;
        };
    } else {
        published = cfr::grader::GsiStrategy::load(CFR_GSI_DATA_DIR);
        if (!published.available()) {
            print_missing_data_help();
            return 1;
        }
        opponent_name = "the Gilpin-Sandholm published equilibrium";
        strategy = [&published](const State& state, std::size_t) {
            return published.action_probabilities(state);
        };
    }

    std::mt19937_64 random_engine(std::random_device{}());
    std::vector<int> deck(cfr::kCardCount);
    std::iota(deck.begin(), deck.end(), 0);

    std::cout
        << "Rhode Island Hold'em against " << opponent_name << ".\n"
        << "Ante 5, bets 10/20/20, three bets per round. Ctrl-D to quit.\n";

    long long hands_played = 0;
    double human_total = 0.0;
    while (true) {
        std::shuffle(deck.begin(), deck.end(), random_engine);
        std::optional<double> human_result =
            play_hand(game, strategy, deck, random_engine);
        if (!human_result) return 0;
        ++hands_played;
        human_total += *human_result;
        report_result(*human_result, hands_played, human_total);
    }
}
