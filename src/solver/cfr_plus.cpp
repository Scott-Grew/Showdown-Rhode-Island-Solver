#include "solver/cfr_plus.hpp"

#include <bit>
#include <cstddef>
#include <cstdint>
#include <fstream>
#include <numeric>
#include <stdexcept>

namespace cfr::solver {

namespace {

constexpr double kStrategySumEpsilon = 1e-12;
constexpr char kCheckpointHeader[] = "CFRPLUS_CHECKPOINT_V1";

void write_table(std::ostream& output, const std::vector<std::vector<double>>& table) {
    for (const std::vector<double>& infoset_values : table) {
        output << infoset_values.size();
        for (double value : infoset_values) {
            output << ' ' << std::hex << std::bit_cast<std::uint64_t>(value) << std::dec;
        }
        output << '\n';
    }
}

std::vector<std::vector<double>> read_table(std::istream& input, std::uint32_t infoset_count) {
    std::vector<std::vector<double>> table(infoset_count);
    for (std::uint32_t infoset_index = 0; infoset_index < infoset_count; ++infoset_index) {
        std::size_t action_count;
        input >> action_count;
        table[infoset_index].resize(action_count);
        for (std::size_t action_index = 0; action_index < action_count; ++action_index) {
            std::uint64_t bits;
            input >> std::hex >> bits >> std::dec;
            table[infoset_index][action_index] = std::bit_cast<double>(bits);
        }
    }
    return table;
}

void expect_token(std::istream& input, const std::string& expected) {
    std::string token;
    input >> token;
    if (token != expected) {
        throw std::runtime_error("cfr_plus checkpoint: expected '" + expected + "', found '" + token + "'");
    }
}

template <typename StrategyFromStored>
void collect_strategy_profile(const game::Game& game, const game::State& state,
                               const std::vector<std::vector<double>>& stored_table,
                               StrategyFromStored&& strategy_from_stored, StrategyProfile& profile) {
    if (game.is_terminal(state)) return;

    if (game.is_chance(state)) {
        for (auto& [action, probability] : game.chance_outcomes(state)) {
            collect_strategy_profile(game, game.apply_action(state, action), stored_table, strategy_from_stored,
                                      profile);
        }
        return;
    }

    const std::vector<double>& stored = stored_table[game.infoset_index(state)];
    if (!stored.empty()) {
        profile[game.infoset_label(state)] = strategy_from_stored(stored);
    }

    for (game::Action action : game.legal_actions(state)) {
        collect_strategy_profile(game, game.apply_action(state, action), stored_table, strategy_from_stored, profile);
    }
}

}

CfrPlus::CfrPlus(const game::Game& game)
    : game_(game),
      cumulative_regrets_(game.infoset_count()),
      regret_snapshot_(game.infoset_count()),
      strategy_sums_(game.infoset_count()) {}

std::array<double, 2> CfrPlus::traverse(const game::State& state, double player0_reach, double player1_reach,
                                         double chance_reach, game::Player updating_player) {
    if (game_.is_terminal(state)) {
        return {game_.terminal_utility(state, 0), game_.terminal_utility(state, 1)};
    }

    if (game_.is_chance(state)) {
        std::array<double, 2> node_value = {0.0, 0.0};
        for (auto& [action, probability] : game_.chance_outcomes(state)) {
            std::array<double, 2> child_value = traverse(game_.apply_action(state, action), player0_reach,
                                                           player1_reach, chance_reach * probability, updating_player);
            node_value[0] += probability * child_value[0];
            node_value[1] += probability * child_value[1];
        }
        return node_value;
    }

    game::Player acting_player = game_.current_player(state);
    std::uint32_t infoset_index = game_.infoset_index(state);
    std::vector<game::Action> actions = game_.legal_actions(state);

    const std::vector<double>& regret_snapshot = regret_snapshot_[infoset_index];
    std::vector<double> strategy = !regret_snapshot.empty()
                                        ? regret_matching_strategy(regret_snapshot)
                                        : regret_matching_strategy(std::vector<double>(actions.size(), 0.0));

    double acting_player_reach = acting_player == 0 ? player0_reach : player1_reach;

    std::array<double, 2> node_value = {0.0, 0.0};
    std::vector<double> acting_player_action_value(actions.size());
    for (std::size_t i = 0; i < actions.size(); ++i) {
        double next_player0_reach = acting_player == 0 ? player0_reach * strategy[i] : player0_reach;
        double next_player1_reach = acting_player == 1 ? player1_reach * strategy[i] : player1_reach;
        std::array<double, 2> child_value = traverse(game_.apply_action(state, actions[i]), next_player0_reach,
                                                       next_player1_reach, chance_reach, updating_player);
        node_value[0] += strategy[i] * child_value[0];
        node_value[1] += strategy[i] * child_value[1];
        acting_player_action_value[i] = child_value[acting_player];
    }

    if (acting_player == updating_player) {
        double opponent_reach = acting_player == 0 ? player1_reach : player0_reach;
        double counterfactual_reach = opponent_reach * chance_reach;
        std::vector<double> increment(actions.size());
        for (std::size_t i = 0; i < actions.size(); ++i) {
            increment[i] = counterfactual_reach * (acting_player_action_value[i] - node_value[acting_player]);
        }

        std::vector<double>& cumulative_regrets = cumulative_regrets_[infoset_index];
        if (cumulative_regrets.empty()) cumulative_regrets.assign(actions.size(), 0.0);
        accumulate_regret_plus(cumulative_regrets, increment);

        std::vector<double>& strategy_sum = strategy_sums_[infoset_index];
        if (strategy_sum.empty()) strategy_sum.assign(actions.size(), 0.0);
        for (std::size_t i = 0; i < actions.size(); ++i) {
            strategy_sum[i] += iteration_ * acting_player_reach * strategy[i];
        }
    }

    return node_value;
}

void CfrPlus::run_iterations(int iteration_count) {
    for (int i = 0; i < iteration_count; ++i) {
        ++iteration_;
        game::Player updating_player = iteration_ % 2;
        regret_snapshot_ = cumulative_regrets_;
        traverse(game_.initial_state(), 1.0, 1.0, 1.0, updating_player);
    }
}

StrategyProfile CfrPlus::current_strategy() const {
    StrategyProfile profile;
    collect_strategy_profile(game_, game_.initial_state(), cumulative_regrets_, regret_matching_strategy, profile);
    return profile;
}

StrategyProfile CfrPlus::average_strategy() const {
    StrategyProfile profile;
    collect_strategy_profile(
        game_, game_.initial_state(), strategy_sums_,
        [](const std::vector<double>& strategy_sum) {
            double total = std::accumulate(strategy_sum.begin(), strategy_sum.end(), 0.0);

            std::vector<double> strategy(strategy_sum.size());
            if (total > kStrategySumEpsilon) {
                for (std::size_t i = 0; i < strategy_sum.size(); ++i) {
                    strategy[i] = strategy_sum[i] / total;
                }
            } else {
                double uniform_probability = 1.0 / static_cast<double>(strategy_sum.size());
                for (double& probability : strategy) {
                    probability = uniform_probability;
                }
            }
            return strategy;
        },
        profile);
    return profile;
}

int CfrPlus::iterations_run() const { return iteration_; }

void CfrPlus::save_checkpoint(const std::string& path) const {
    std::ofstream output(path);
    if (!output) {
        throw std::runtime_error("cfr_plus checkpoint: could not open '" + path + "' for writing");
    }

    output << kCheckpointHeader << '\n';
    output << "iteration " << iteration_ << '\n';
    output << "infoset_count " << cumulative_regrets_.size() << '\n';
    output << "regrets\n";
    write_table(output, cumulative_regrets_);
    output << "strategy_sums\n";
    write_table(output, strategy_sums_);
}

CfrPlus CfrPlus::load_checkpoint(const game::Game& game, const std::string& path) {
    std::ifstream input(path);
    if (!input) {
        throw std::runtime_error("cfr_plus checkpoint: could not open '" + path + "' for reading");
    }

    expect_token(input, kCheckpointHeader);

    expect_token(input, "iteration");
    int iteration;
    input >> iteration;

    expect_token(input, "infoset_count");
    std::uint32_t infoset_count;
    input >> infoset_count;
    if (infoset_count != game.infoset_count()) {
        throw std::runtime_error("cfr_plus checkpoint: infoset_count mismatch (checkpoint has " +
                                  std::to_string(infoset_count) + ", game has " +
                                  std::to_string(game.infoset_count()) + ")");
    }

    expect_token(input, "regrets");
    std::vector<std::vector<double>> cumulative_regrets = read_table(input, infoset_count);

    expect_token(input, "strategy_sums");
    std::vector<std::vector<double>> strategy_sums = read_table(input, infoset_count);

    CfrPlus solver(game);
    solver.iteration_ = iteration;
    solver.cumulative_regrets_ = std::move(cumulative_regrets);
    solver.strategy_sums_ = std::move(strategy_sums);
    return solver;
}

}
