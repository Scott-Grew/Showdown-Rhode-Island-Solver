#pragma once

#include <array>
#include <cassert>
#include <cstddef>
#include <cstdint>
#include <fstream>
#include <random>
#include <stdexcept>
#include <string>
#include <span>
#include <vector>

#include "game/game.hpp"
#include "game/game_concept.hpp"
#include "solver/cfr_solver.hpp"
#include "solver/strategy.hpp"

namespace cfr::solver {

template <typename GameT>
requires game::GameLike<GameT>
class ExternalSamplingSolver {
public:
    ExternalSamplingSolver(const GameT& game, std::uint64_t seed)
        : game_(game),
          cumulative_regrets_(static_cast<std::size_t>(game.infoset_count()) * kStride, 0.0),
          strategy_sums_(static_cast<std::size_t>(game.infoset_count()) * kStride, 0.0),
          random_engine_(seed) {}

    void run_iterations(int iteration_count) {
        for (int i = 0; i < iteration_count; ++i) {
            ++iteration_;
            for (game::Player traverser = 0; traverser < 2; ++traverser) {
                traverse(game_.initial_state(), traverser);
            }
        }
    }

    int iterations_run() const { return iteration_; }

    void average_strategy_into(std::uint32_t infoset_index, std::size_t action_count,
                                std::span<double> strategy) const {
        std::size_t offset = static_cast<std::size_t>(infoset_index) * kStride;
        double total = 0.0;
        for (std::size_t i = 0; i < action_count; ++i) total += strategy_sums_[offset + i];

        if (total <= detail::kStrategySumEpsilon) {
            for (std::size_t i = 0; i < action_count; ++i) {
                strategy[i] = 1.0 / static_cast<double>(action_count);
            }
            return;
        }
        for (std::size_t i = 0; i < action_count; ++i) strategy[i] = strategy_sums_[offset + i] / total;
    }

    void save_strategy_sums(const std::string& path) const {
        std::ofstream output(path, std::ios::binary);
        if (!output) throw std::runtime_error("mccfr: could not open '" + path + "' for writing");
        std::uint64_t value_count = strategy_sums_.size();
        output.write(reinterpret_cast<const char*>(&value_count), sizeof(value_count));
        output.write(reinterpret_cast<const char*>(&iteration_), sizeof(iteration_));
        output.write(reinterpret_cast<const char*>(strategy_sums_.data()),
                     static_cast<std::streamsize>(value_count * sizeof(double)));
        output.flush();
        if (!output) throw std::runtime_error("mccfr: write to '" + path + "' failed, strategy is incomplete");
    }

    void load_strategy_sums(const std::string& path) {
        std::ifstream input(path, std::ios::binary);
        if (!input) throw std::runtime_error("mccfr: could not open '" + path + "' for reading");
        std::uint64_t value_count = 0;
        input.read(reinterpret_cast<char*>(&value_count), sizeof(value_count));
        input.read(reinterpret_cast<char*>(&iteration_), sizeof(iteration_));
        if (!input) throw std::runtime_error("mccfr: '" + path + "' is truncated inside the header");
        if (value_count != strategy_sums_.size()) {
            throw std::runtime_error("mccfr: strategy table size mismatch");
        }
        input.read(reinterpret_cast<char*>(strategy_sums_.data()),
                   static_cast<std::streamsize>(value_count * sizeof(double)));
        if (!input) throw std::runtime_error("mccfr: '" + path + "' is truncated inside the strategy table");
        if (input.peek() != std::char_traits<char>::eof()) {
            throw std::runtime_error("mccfr: '" + path + "' has trailing bytes after the strategy table");
        }
    }

    StrategyProfile average_strategy() const {
        StrategyProfile profile;
        detail::collect_strategy_profile(
            game_, game_.initial_state(), strategy_sums_, kStride,
            [](std::span<const double> strategy_sum) {
                double total = 0.0;
                for (double value : strategy_sum) total += value;

                std::vector<double> strategy(strategy_sum.size());
                if (total > detail::kStrategySumEpsilon) {
                    for (std::size_t i = 0; i < strategy_sum.size(); ++i) strategy[i] = strategy_sum[i] / total;
                } else {
                    for (double& probability : strategy) {
                        probability = 1.0 / static_cast<double>(strategy_sum.size());
                    }
                }
                return strategy;
            },
            profile);
        return profile;
    }

private:
    static constexpr std::size_t kStride = GameT::kMaxActions;

    std::size_t sample_index(std::span<const double> distribution) {
        double roll = std::uniform_real_distribution<double>(0.0, 1.0)(random_engine_);
        double cumulative = 0.0;
        for (std::size_t i = 0; i + 1 < distribution.size(); ++i) {
            cumulative += distribution[i];
            if (roll < cumulative) return i;
        }
        return distribution.size() - 1;
    }

    double traverse(const game::State& state, game::Player traverser) {
        if (game_.is_terminal(state)) return game_.terminal_utility(state, traverser);

        if (game_.is_chance(state)) {
            std::vector<std::pair<game::Action, double>> outcomes = game_.chance_outcomes(state);
            std::vector<double> probabilities(outcomes.size());
            for (std::size_t i = 0; i < outcomes.size(); ++i) probabilities[i] = outcomes[i].second;
            std::size_t sampled = sample_index(probabilities);
            return traverse(game_.apply_action(state, outcomes[sampled].first), traverser);
        }

        game::Player acting_player = game_.current_player(state);
        std::vector<game::Action> actions = game_.legal_actions(state);
        assert(actions.size() <= detail::kMaxActionsPerInfoset);

        std::size_t offset = static_cast<std::size_t>(game_.infoset_index(state)) * kStride;
        std::array<double, detail::kMaxActionsPerInfoset> strategy{};
        regret_matching_strategy_into(
            std::span<const double>(cumulative_regrets_.data() + offset, actions.size()),
            std::span<double>(strategy.data(), actions.size()));

        if (acting_player != traverser) {
            for (std::size_t i = 0; i < actions.size(); ++i) strategy_sums_[offset + i] += strategy[i];
            std::size_t sampled = sample_index(std::span<const double>(strategy.data(), actions.size()));
            return traverse(game_.apply_action(state, actions[sampled]), traverser);
        }

        std::array<double, detail::kMaxActionsPerInfoset> action_value{};
        double node_value = 0.0;
        for (std::size_t i = 0; i < actions.size(); ++i) {
            action_value[i] = traverse(game_.apply_action(state, actions[i]), traverser);
            node_value += strategy[i] * action_value[i];
        }
        for (std::size_t i = 0; i < actions.size(); ++i) {
            cumulative_regrets_[offset + i] += action_value[i] - node_value;
        }
        return node_value;
    }

    const GameT& game_;
    std::vector<double> cumulative_regrets_;
    std::vector<double> strategy_sums_;
    std::mt19937_64 random_engine_;
    int iteration_ = 0;
};

}
