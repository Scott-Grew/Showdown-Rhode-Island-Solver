#pragma once

#include <array>
#include <atomic>
#include <cassert>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <fstream>
#include <random>
#include <span>
#include <stdexcept>
#include <string>
#include <memory>
#include <thread>
#include <vector>

#include "game/game.hpp"
#include "game/game_concept.hpp"
#include "solver/cfr_solver.hpp"
#include "solver/strategy.hpp"

namespace cfr::solver {

static_assert(std::atomic<double>::is_always_lock_free);

inline std::uint64_t scrambled_seed(std::uint64_t seed, std::size_t thread_index) {
    if (thread_index == 0) return seed;
    std::uint64_t value = seed + thread_index + 0x9e3779b97f4a7c15ULL;
    value = (value ^ (value >> 30)) * 0xbf58476d1ce4e5b9ULL;
    value = (value ^ (value >> 27)) * 0x94d049bb133111ebULL;
    return value ^ (value >> 31);
}

template <typename GameT>
requires game::GameLike<GameT>
class ExternalSamplingSolver {
public:
    ExternalSamplingSolver(const GameT& game, std::uint64_t seed)
        : game_(game),
          cumulative_regrets_(static_cast<std::size_t>(game.infoset_count()) * kStride),
          strategy_sums_(static_cast<std::size_t>(game.infoset_count()) * kStride),
          seed_(seed) {}

    void run_iterations(int iteration_count, int thread_count = 1) {
        if (iteration_count < 0) throw std::invalid_argument("mccfr: iteration_count must not be negative");
        if (iteration_count == 0) return;
        if (thread_count <= 0) throw std::invalid_argument("mccfr: thread_count must be positive");

        while (contexts_.size() < static_cast<std::size_t>(thread_count)) {
            contexts_.push_back(std::make_unique<TraversalContext>(scrambled_seed(seed_, contexts_.size())));
        }

        if (thread_count == 1) {
            run_iteration_range<false>(iteration_count, *contexts_[0]);
        } else {
            std::vector<std::thread> workers;
            workers.reserve(static_cast<std::size_t>(thread_count));
            int base_share = iteration_count / thread_count;
            int remainder = iteration_count % thread_count;
            for (int index = 0; index < thread_count; ++index) {
                int share = base_share + (index < remainder ? 1 : 0);
                workers.emplace_back([this, share, index] {
                    run_iteration_range<true>(share, *contexts_[static_cast<std::size_t>(index)]);
                });
            }
            for (std::thread& worker : workers) worker.join();
        }
        iteration_ += iteration_count;
    }

    int iterations_run() const { return iteration_; }

    double strategy_mass() const {
        long double total = 0.0L;
        for (const std::atomic<double>& value : strategy_sums_) total += value.load(std::memory_order_relaxed);
        return static_cast<double>(total);
    }

    std::uint64_t opponent_node_visits() const {
        std::uint64_t total = 0;
        for (const std::unique_ptr<TraversalContext>& context : contexts_) total += context->opponent_node_visits;
        return total;
    }

    void average_strategy_into(std::uint32_t infoset_index, std::size_t action_count,
                                std::span<double> strategy) const {
        std::size_t offset = static_cast<std::size_t>(infoset_index) * kStride;
        for (std::size_t i = 0; i < action_count; ++i) {
            strategy[i] = strategy_sums_[offset + i].load(std::memory_order_relaxed);
        }
        normalize_or_uniform(strategy.subspan(0, action_count));
    }

    void save_strategy_sums(const std::string& path) const {
        std::ofstream output(path, std::ios::binary);
        if (!output) throw std::runtime_error("mccfr: could not open '" + path + "' for writing");
        std::vector<double> snapshot = table_snapshot(strategy_sums_);
        std::uint64_t value_count = snapshot.size();
        output.write(reinterpret_cast<const char*>(&value_count), sizeof(value_count));
        output.write(reinterpret_cast<const char*>(&iteration_), sizeof(iteration_));
        output.write(reinterpret_cast<const char*>(snapshot.data()),
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
        std::vector<double> loaded(static_cast<std::size_t>(value_count));
        input.read(reinterpret_cast<char*>(loaded.data()),
                   static_cast<std::streamsize>(value_count * sizeof(double)));
        if (!input) throw std::runtime_error("mccfr: '" + path + "' is truncated inside the strategy table");
        if (input.peek() != std::char_traits<char>::eof()) {
            throw std::runtime_error("mccfr: '" + path + "' has trailing bytes after the strategy table");
        }
        for (std::size_t i = 0; i < loaded.size(); ++i) {
            strategy_sums_[i].store(loaded[i], std::memory_order_relaxed);
        }
    }

    StrategyProfile average_strategy() const {
        StrategyProfile profile;
        std::vector<double> snapshot = table_snapshot(strategy_sums_);
        detail::collect_strategy_profile(game_, game_.initial_state(), snapshot, kStride, average_from_sums, profile);
        return profile;
    }

private:
    static constexpr std::size_t kStride = GameT::kMaxActions;

    struct TraversalContext {
        explicit TraversalContext(std::uint64_t seed) : random_engine(seed) {}
        std::mt19937_64 random_engine;
        std::uint64_t opponent_node_visits = 0;
    };

    static std::vector<double> table_snapshot(const std::vector<std::atomic<double>>& table) {
        std::vector<double> snapshot(table.size());
        for (std::size_t i = 0; i < table.size(); ++i) snapshot[i] = table[i].load(std::memory_order_relaxed);
        return snapshot;
    }

    template <bool Concurrent>
    void run_iteration_range(int iteration_count, TraversalContext& context) {
        for (int i = 0; i < iteration_count; ++i) {
            for (game::Player traverser = 0; traverser < 2; ++traverser) {
                traverse<Concurrent>(game_.initial_state(), traverser, context);
            }
        }
    }

    template <bool Concurrent>
    static void accumulate(std::atomic<double>& slot, double increment) {
        if constexpr (Concurrent) {
            slot.fetch_add(increment, std::memory_order_relaxed);
        } else {
            slot.store(slot.load(std::memory_order_relaxed) + increment, std::memory_order_relaxed);
        }
    }

    static std::size_t sample_index(std::span<const double> distribution, std::mt19937_64& random_engine) {
        double roll = std::uniform_real_distribution<double>(0.0, 1.0)(random_engine);
        double cumulative = 0.0;
        for (std::size_t i = 0; i + 1 < distribution.size(); ++i) {
            cumulative += distribution[i];
            if (roll < cumulative) return i;
        }
        return distribution.size() - 1;
    }

    template <bool Concurrent>
    double traverse(const game::State& state, game::Player traverser, TraversalContext& context) {
        if (game_.is_terminal(state)) return game_.terminal_utility(state, traverser);

        if (game_.is_chance(state)) {
            std::vector<std::pair<game::Action, double>> outcomes = game_.chance_outcomes(state);
            std::vector<double> probabilities(outcomes.size());
            for (std::size_t i = 0; i < outcomes.size(); ++i) probabilities[i] = outcomes[i].second;
            std::size_t sampled = sample_index(probabilities, context.random_engine);
            return traverse<Concurrent>(game_.apply_action(state, outcomes[sampled].first), traverser, context);
        }

        game::Player acting_player = game_.current_player(state);
        std::vector<game::Action> actions = game_.legal_actions(state);
        assert(actions.size() <= detail::kMaxActionsPerInfoset);

        std::size_t offset = static_cast<std::size_t>(game_.infoset_index(state)) * kStride;
        std::array<double, detail::kMaxActionsPerInfoset> strategy{};
        for (std::size_t i = 0; i < actions.size(); ++i) {
            strategy[i] = cumulative_regrets_[offset + i].load(std::memory_order_relaxed);
        }
        regret_matching_in_place(std::span<double>(strategy.data(), actions.size()));

        if (acting_player != traverser) {
            ++context.opponent_node_visits;
            for (std::size_t i = 0; i < actions.size(); ++i) {
                accumulate<Concurrent>(strategy_sums_[offset + i], strategy[i]);
            }
            std::size_t sampled = sample_index(std::span<const double>(strategy.data(), actions.size()),
                                                context.random_engine);
            return traverse<Concurrent>(game_.apply_action(state, actions[sampled]), traverser, context);
        }

        std::array<double, detail::kMaxActionsPerInfoset> action_value{};
        double node_value = 0.0;
        for (std::size_t i = 0; i < actions.size(); ++i) {
            action_value[i] = traverse<Concurrent>(game_.apply_action(state, actions[i]), traverser, context);
            node_value += strategy[i] * action_value[i];
        }
        double weighted_residual = 0.0;
        for (std::size_t i = 0; i < actions.size(); ++i) {
            weighted_residual += strategy[i] * (action_value[i] - node_value);
            accumulate<Concurrent>(cumulative_regrets_[offset + i], action_value[i] - node_value);
        }
        assert(std::abs(weighted_residual) < 1e-6);
        static_cast<void>(weighted_residual);
        return node_value;
    }

    const GameT& game_;
    std::vector<std::atomic<double>> cumulative_regrets_;
    std::vector<std::atomic<double>> strategy_sums_;
    std::vector<std::unique_ptr<TraversalContext>> contexts_;
    std::uint64_t seed_ = 0;
    int iteration_ = 0;
};

}
