#include <algorithm>
#include <functional>
#include <iostream>
#include <numeric>
#include <random>
#include <string>
#include <vector>

#include "game/rhode_island.hpp"
#include "grader/gsi_strategy.hpp"
#include "solver/mccfr_solver.hpp"

using namespace cfr::game;

namespace {

constexpr Player kHuman = 0;
constexpr Player kSolver = 1;

std::string action_prompt(Action action, const State& state) {
    ParsedRounds parsed = parse_rounds(state);
    const std::vector<Action>& round_actions = active_round_actions(parsed);
    bool facing_wager = !round_actions.empty() && round_actions.back() == kActionRaise;
    switch (action) {
        case kActionFold: return "fold";
        case kActionCallCheck: return facing_wager ? "call" : "check";
        case kActionRaise: return facing_wager ? "raise" : "bet";
    }
    return "?";
}

void show_table(const State& state, bool reveal_solver_hole) {
    std::cout << "\n  your card " << rih_card_name(state.private_cards[kHuman]);
    if (state.public_count > 0) {
        std::cout << "   board";
        for (std::uint8_t i = 0; i < state.public_count; ++i) {
            std::cout << " " << rih_card_name(state.public_cards[i]);
        }
    }
    if (reveal_solver_hole) std::cout << "   solver card " << rih_card_name(state.private_cards[kSolver]);
    std::array<int, 2> contribution = rih_contributions(state);
    std::cout << "   pot " << contribution[0] + contribution[1] << "\n";
}

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
        std::transform(typed.begin(), typed.end(), typed.begin(), [](unsigned char c) { return std::tolower(c); });
        for (Action action : actions) {
            if (action_prompt(action, state) == typed) return action;
        }
        std::cout << "  not a legal move here\n";
    }
}

using ActionProbabilities = std::function<std::vector<double>(const State&, std::size_t)>;

Action solver_move(const RhodeIslandGame& game, const State& state, const ActionProbabilities& strategy,
                    std::mt19937_64& random_engine) {
    std::vector<Action> actions = game.legal_actions(state);
    std::vector<double> probabilities = strategy(state, actions.size());
    double roll = std::uniform_real_distribution<double>(0.0, 1.0)(random_engine);
    double cumulative = 0.0;
    for (std::size_t i = 0; i + 1 < actions.size(); ++i) {
        cumulative += probabilities[i];
        if (roll < cumulative) return actions[i];
    }
    return actions.back();
}

}

int main(int argc, char** argv) {
    RhodeIslandGame game;
    ActionProbabilities strategy;
    std::string opponent_name;

    cfr::solver::ExternalSamplingSolver<RhodeIslandGame> solved(game, 1);
    cfr::grader::GsiStrategy published;

    if (argc > 1) {
        solved.load_strategy_sums(argv[1]);
        opponent_name = "our solver, " + std::to_string(solved.iterations_run()) + " iterations";
        strategy = [&game, &solved](const State& state, std::size_t action_count) {
            std::vector<double> probabilities(action_count);
            solved.average_strategy_into(game.infoset_index(state), action_count, probabilities);
            return probabilities;
        };
    } else {
        published = cfr::grader::GsiStrategy::load(CFR_GSI_DATA_DIR);
        if (!published.available()) {
            std::cerr << "no equilibrium data in " << CFR_GSI_DATA_DIR << "\n"
                      << "pass a solved strategy file instead, or fetch theirs with:\n"
                      << "  curl -O http://www.cs.cmu.edu/~gilpin/GSI.jar\n"
                      << "  unzip -j GSI.jar 'strategy/*' -d data/gsi\n";
            return 1;
        }
        opponent_name = "the Gilpin-Sandholm published equilibrium";
        strategy = [&published](const State& state, std::size_t) {
            return published.action_probabilities(state);
        };
    }

    std::mt19937_64 random_engine(std::random_device{}());
    std::vector<int> deck(kRihDeckSize);
    std::iota(deck.begin(), deck.end(), 0);

    std::cout << "Rhode Island Hold'em against " << opponent_name << ".\n"
              << "Ante 5, bets 10/20/20, three bets per round. Ctrl-D to quit.\n";

    long long hands_played = 0;
    double human_total = 0.0;

    while (true) {
        std::shuffle(deck.begin(), deck.end(), random_engine);
        State state = game.initial_state();
        state = game.apply_action(state, kChanceCardOffset + deck[0]);
        state = game.apply_action(state, kChanceCardOffset + deck[1]);
        int next_board_card = 2;

        while (!game.is_terminal(state)) {
            if (game.is_chance(state)) {
                state = game.apply_action(state, kChanceCardOffset + deck[next_board_card++]);
                show_table(state, false);
                continue;
            }
            if (game.current_player(state) == kHuman) {
                if (state.history_len == 2 && state.public_count == 0) show_table(state, false);
                Action chosen = ask_human(game, state);
                if (!std::cin) return 0;
                state = game.apply_action(state, chosen);
                continue;
            }
            Action chosen = solver_move(game, state, strategy, random_engine);
            std::cout << "  solver " << action_prompt(chosen, state) << "\n";
            state = game.apply_action(state, chosen);
        }

        double human_result = game.terminal_utility(state, kHuman);
        ++hands_played;
        human_total += human_result;
        show_table(state, true);
        std::cout << "  you " << (human_result > 0 ? "win " : human_result < 0 ? "lose " : "push")
                  << (human_result == 0 ? std::string()
                                        : std::to_string(static_cast<long long>(std::abs(human_result))))
                  << "\n  after " << hands_played << " hands you are " << (human_total >= 0 ? "+" : "")
                  << static_cast<long long>(human_total) << " (" << human_total / static_cast<double>(hands_played)
                  << " per hand)\n";
    }
}
