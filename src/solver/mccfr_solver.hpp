// External-sampling Monte Carlo CFR, the solver that trains Rhode
// Island. Its tables are lock-free atomics shared by the worker
// threads, and it saves and loads the strategy checkpoint.

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
#include "solver/cfr_solver.hpp"
#include "solver/strategy.hpp"

namespace cfr::solver {

static_assert(std::atomic<double>::is_always_lock_free);

// Seed for one worker: thread 0 keeps the seed, the others get a
// splitmix64 mix of seed + index so their streams differ.
inline std::uint64_t scrambled_seed(std::uint64_t seed,
                                    std::size_t thread_index) {
    if (thread_index == 0) return seed;
    std::uint64_t value = seed + thread_index + 0x9e3779b97f4a7c15ULL;
    value = (value ^ (value >> 30)) * 0xbf58476d1ce4e5b9ULL;
    value = (value ^ (value >> 27)) * 0x94d049bb133111ebULL;
    return value ^ (value >> 31);
}

// External-sampling MCCFR. Tables are atomic doubles shared by all
// worker threads; the game is held by reference.
template <typename GameT>
    requires game::GameLike<GameT>
class ExternalSamplingSolver {
public:
    // Allocates zeroed tables; seed fixes
    // every worker's random stream.
    ExternalSamplingSolver(const GameT& game, std::uint64_t seed)
        : game_(game),
          cumulative_regrets_(static_cast<std::size_t>(game.infoset_count()) *
                              kStride),
          strategy_sums_(static_cast<std::size_t>(game.infoset_count()) *
                         kStride),
          seed_(seed) {}

    // Runs this many more iterations, split across thread_count
    // threads; each thread keeps its random engine between calls.
    void run_iterations(int iteration_count, int thread_count = 1) {
        if (iteration_count < 0)
            throw std::invalid_argument(
                "mccfr: iteration_count must not be negative");
        if (iteration_count == 0) return;
        if (thread_count <= 0)
            throw std::invalid_argument("mccfr: thread_count must be positive");

        while (contexts_.size() < static_cast<std::size_t>(thread_count)) {
            contexts_.push_back(std::make_unique<TraversalContext>(
                scrambled_seed(seed_, contexts_.size())));
        }

        // One thread skips the atomic read-modify-write path.
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
                    run_iteration_range<true>(
                        share, *contexts_[static_cast<std::size_t>(index)]);
                });
            }
            for (std::thread& worker : workers) worker.join();
        }
        iteration_ += iteration_count;
    }

    // Iterations completed, including any loaded from a checkpoint.
    int iterations_run() const { return iteration_; }

    // Sum of the whole strategy table; each opponent-node visit adds
    // a distribution summing to 1.
    double strategy_mass() const {
        long double total = 0.0L;
        for (const std::atomic<double>& value : strategy_sums_)
            total += value.load(std::memory_order_relaxed);
        return static_cast<double>(total);
    }

    // Opponent decision nodes visited by all workers since
    // construction. Call only while no run is in flight.
    std::uint64_t opponent_node_visits() const {
        std::uint64_t total = 0;
        for (const std::unique_ptr<TraversalContext>& context : contexts_)
            total += context->opponent_node_visits;
        return total;
    }

    // Writes one infoset's average strategy into the first
    // action_count entries of strategy.
    void average_strategy_into(std::uint32_t infoset_index,
                               std::size_t action_count,
                               std::span<double> strategy) const {
        std::size_t table_offset =
            static_cast<std::size_t>(infoset_index) * kStride;
        for (std::size_t i = 0; i < action_count; ++i) {
            strategy[i] = strategy_sums_[table_offset + i].load(
                std::memory_order_relaxed);
        }
        normalize_or_uniform(strategy.subspan(0, action_count));
    }

    // Binary checkpoint in native layout: uint64 count, int
    // iterations, then the sums as doubles. Regrets are not saved.
    void save_strategy_sums(const std::string& path) const {
        std::ofstream output(path, std::ios::binary);
        if (!output)
            throw std::runtime_error("mccfr: could not open '" + path +
                                     "' for writing");
        std::vector<double> snapshot = table_snapshot(strategy_sums_);
        std::uint64_t value_count = snapshot.size();
        output.write(reinterpret_cast<const char*>(&value_count),
                     sizeof(value_count));
        output.write(reinterpret_cast<const char*>(&iteration_),
                     sizeof(iteration_));
        output.write(
            reinterpret_cast<const char*>(snapshot.data()),
            static_cast<std::streamsize>(value_count * sizeof(double)));
        output.flush();
        if (!output)
            throw std::runtime_error("mccfr: write to '" + path +
                                     "' failed, strategy is incomplete");
    }

    // Reads a checkpoint and rejects a wrong size, a truncated file
    // or trailing bytes. Regrets stay as they were.
    void load_strategy_sums(const std::string& path) {
        std::ifstream input(path, std::ios::binary);
        if (!input)
            throw std::runtime_error("mccfr: could not open '" + path +
                                     "' for reading");
        std::uint64_t value_count = 0;
        input.read(reinterpret_cast<char*>(&value_count), sizeof(value_count));
        input.read(reinterpret_cast<char*>(&iteration_), sizeof(iteration_));
        if (!input)
            throw std::runtime_error("mccfr: '" + path +
                                     "' is truncated inside the header");
        if (value_count != strategy_sums_.size()) {
            throw std::runtime_error("mccfr: strategy table size mismatch");
        }
        std::vector<double> loaded(static_cast<std::size_t>(value_count));
        input.read(reinterpret_cast<char*>(loaded.data()),
                   static_cast<std::streamsize>(value_count * sizeof(double)));
        if (!input)
            throw std::runtime_error(
                "mccfr: '" + path + "' is truncated inside the strategy table");
        if (input.peek() != std::char_traits<char>::eof()) {
            throw std::runtime_error(
                "mccfr: '" + path +
                "' has trailing bytes after the strategy table");
        }
        for (std::size_t i = 0; i < loaded.size(); ++i) {
            strategy_sums_[i].store(loaded[i], std::memory_order_relaxed);
        }
    }

    // Average strategy of every reachable infoset, keyed by label.
    StrategyProfile average_strategy() const {
        StrategyProfile profile;
        std::vector<double> snapshot = table_snapshot(strategy_sums_);
        detail::collect_strategy_profile(game_, game_.initial_state(), snapshot,
                                         kStride, average_from_sums, profile);
        return profile;
    }

private:
    static constexpr std::size_t kStride = GameT::kMaxActions;

    // Largest weighted regret residual the debug assert accepts.
    static constexpr double kRegretResidualTolerance = 1e-6;

    // State owned by one worker thread: its
    // random engine and its visit counter.
    struct TraversalContext {
        // Seeds this worker's engine.
        explicit TraversalContext(std::uint64_t seed) : random_engine(seed) {}
        std::mt19937_64 random_engine;
        std::uint64_t opponent_node_visits = 0;
    };

    // Plain copy of an atomic table, read entry by entry.
    static std::vector<double> table_snapshot(
        const std::vector<std::atomic<double>>& table) {
        std::vector<double> snapshot(table.size());
        for (std::size_t i = 0; i < table.size(); ++i)
            snapshot[i] = table[i].load(std::memory_order_relaxed);
        return snapshot;
    }

    // Runs iterations on one context; an iteration is one traversal
    // for each player.
    template <bool Concurrent>
    void run_iteration_range(int iteration_count, TraversalContext& context) {
        for (int i = 0; i < iteration_count; ++i) {
            for (game::Player traverser = 0; traverser < 2; ++traverser) {
                traverse<Concurrent>(game_.initial_state(), traverser, context);
            }
        }
    }

    // Adds to a table entry: atomically when threads share the
    // table, as a plain load and store when only one thread runs.
    template <bool Concurrent>
    static void accumulate(std::atomic<double>& table_entry, double increment) {
        if constexpr (Concurrent) {
            table_entry.fetch_add(increment, std::memory_order_relaxed);
        } else {
            table_entry.store(
                table_entry.load(std::memory_order_relaxed) + increment,
                std::memory_order_relaxed);
        }
    }

    // Samples an index from a distribution summing to 1; the last
    // index absorbs any rounding shortfall.
    static std::size_t sample_index(std::span<const double> distribution,
                                    std::mt19937_64& random_engine) {
        double roll =
            std::uniform_real_distribution<double>(0.0, 1.0)(random_engine);
        double cumulative = 0.0;
        for (std::size_t i = 0; i + 1 < distribution.size(); ++i) {
            cumulative += distribution[i];
            if (roll < cumulative) return i;
        }
        return distribution.size() - 1;
    }

    // Samples one outcome of a chance node by its probability.
    game::Action sample_chance_action(const game::State& state,
                                      TraversalContext& context) const {
        std::vector<game::ChanceOutcome> outcomes =
            game_.chance_outcomes(state);
        std::vector<double> probabilities(outcomes.size());
        for (std::size_t i = 0; i < outcomes.size(); ++i) {
            probabilities[i] = outcomes[i].probability;
        }
        return outcomes[sample_index(probabilities, context.random_engine)]
            .action;
    }

    // Regret-matching strategy of one infoset, read from the live
    // regret table into strategy, one entry per legal action.
    void current_strategy_into(std::size_t table_offset,
                               std::span<double> strategy) const {
        for (std::size_t i = 0; i < strategy.size(); ++i) {
            strategy[i] = cumulative_regrets_[table_offset + i].load(
                std::memory_order_relaxed);
        }
        regret_matching_in_place(strategy);
    }

    // Value of state for the traverser. Chance and opponent nodes
    // sample one child; traverser nodes try every action.
    template <bool Concurrent>
    double traverse(const game::State& state, game::Player traverser,
                    TraversalContext& context) {
        if (game_.is_terminal(state))
            return game_.terminal_utility(state, traverser);

        if (game_.is_chance(state)) {
            game::Action dealt = sample_chance_action(state, context);
            return traverse<Concurrent>(game_.apply_action(state, dealt),
                                        traverser, context);
        }

        game::Player acting_player = game_.current_player(state);
        std::vector<game::Action> actions = game_.legal_actions(state);
        assert(actions.size() <= detail::kMaxActionsPerInfoset);

        std::size_t table_offset =
            static_cast<std::size_t>(game_.infoset_index(state)) * kStride;
        std::array<double, detail::kMaxActionsPerInfoset> strategy{};
        current_strategy_into(
            table_offset, std::span<double>(strategy.data(), actions.size()));

        // The opponent's current strategy feeds the average here.
        if (acting_player != traverser) {
            ++context.opponent_node_visits;
            for (std::size_t i = 0; i < actions.size(); ++i) {
                accumulate<Concurrent>(strategy_sums_[table_offset + i],
                                       strategy[i]);
            }
            std::size_t sampled = sample_index(
                std::span<const double>(strategy.data(), actions.size()),
                context.random_engine);
            return traverse<Concurrent>(
                game_.apply_action(state, actions[sampled]), traverser,
                context);
        }

        std::array<double, detail::kMaxActionsPerInfoset> action_value{};
        double node_value = 0.0;
        for (std::size_t i = 0; i < actions.size(); ++i) {
            action_value[i] = traverse<Concurrent>(
                game_.apply_action(state, actions[i]), traverser, context);
            node_value += strategy[i] * action_value[i];
        }
        // Regrets weighted by the strategy sum to zero by
        // construction; the assert checks that identity.
        [[maybe_unused]] double weighted_residual = 0.0;
        for (std::size_t i = 0; i < actions.size(); ++i) {
            weighted_residual += strategy[i] * (action_value[i] - node_value);
            accumulate<Concurrent>(cumulative_regrets_[table_offset + i],
                                   action_value[i] - node_value);
        }
        assert(std::abs(weighted_residual) < kRegretResidualTolerance);
        return node_value;
    }

    // Both tables share one flat layout, kStride entries per infoset:
    //
    //   entry = infoset_index * kStride + position in legal_actions
    //
    //   | infoset 0      | infoset 1      | ...
    //   | a0 | a1 | a2   | a0 | a1 | a2   |
    //
    // An infoset with fewer legal actions than kStride never writes
    // its trailing entries, so they stay zero.
    const GameT& game_;
    std::vector<std::atomic<double>> cumulative_regrets_;
    std::vector<std::atomic<double>> strategy_sums_;
    std::vector<std::unique_ptr<TraversalContext>> contexts_;
    std::uint64_t seed_ = 0;
    int iteration_ = 0;
};

}
