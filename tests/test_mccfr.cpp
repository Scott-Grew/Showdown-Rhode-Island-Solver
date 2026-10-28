#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_string.hpp>

#include <cmath>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <set>
#include <vector>

#include "game/rhode_island.hpp"
#include "oracles/best_response.hpp"
#include "oracles/kuhn.hpp"
#include "oracles/leduc.hpp"
#include "solver/mccfr_solver.hpp"
#include "tree_walk.hpp"

using namespace cfr::game;
using cfr::solver::exploitability;
using cfr::solver::ExternalSamplingSolver;
using cfr::solver::StrategyProfile;

namespace {

// Label-keyed average strategy. The solver stores sums by infoset
// index; the oracle best response looks them up by label.
template <typename GameT>
StrategyProfile average_strategy_profile(
    const ExternalSamplingSolver<GameT>& solver, const GameT& game) {
    StrategyProfile profile;
    walk(game, game.initial_state(), [&](const State& state) {
        if (game.is_terminal(state) || game.is_chance(state)) return;
        auto actions = game.legal_actions(state);
        std::vector<double> strategy(actions.size());
        solver.average_strategy_into(game.infoset_index(state), actions.size(),
                                     strategy);
        profile[game.infoset_label(state)] = strategy;
    });
    return profile;
}

}

TEST_CASE("external sampling kuhn exploitability is below 0.01") {
    KuhnGame game;
    ExternalSamplingSolver<KuhnGame> solver(game, 20260726);
    solver.run_iterations(200000);

    REQUIRE(exploitability(game, average_strategy_profile(solver, game)) <
            1e-2);
}

TEST_CASE("external sampling leduc exploitability is below 0.1") {
    LeducGame game;
    ExternalSamplingSolver<LeducGame> solver(game, 20260726);
    solver.run_iterations(200000);

    REQUIRE(exploitability(game, average_strategy_profile(solver, game)) < 0.1);
}

TEST_CASE("strategy mass equals opponent node visits at every thread count") {
    for (int thread_count : {1, 2, 4}) {
        LeducGame game;
        ExternalSamplingSolver<LeducGame> solver(game, 20260726);
        solver.run_iterations(20000, thread_count);

        double mass = solver.strategy_mass();
        double visits = static_cast<double>(solver.opponent_node_visits());
        REQUIRE(visits > 0.0);
        REQUIRE(std::abs(mass - visits) / visits < 1e-9);
    }
}

TEST_CASE("two one-thread mccfr runs match every average strategy") {
    LeducGame game;
    ExternalSamplingSolver<LeducGame> single(game, 20260726);
    ExternalSamplingSolver<LeducGame> repeat(game, 20260726);
    single.run_iterations(5000, 1);
    repeat.run_iterations(5000, 1);

    std::vector<std::size_t> action_counts(game.infoset_count(), 0);
    walk(game, game.initial_state(), [&](const State& state) {
        if (game.is_terminal(state) || game.is_chance(state)) return;
        action_counts[game.infoset_index(state)] =
            game.legal_actions(state).size();
    });

    for (std::uint32_t index = 0; index < game.infoset_count(); ++index) {
        std::size_t action_count = action_counts[index];
        REQUIRE(action_count >= 2);
        std::vector<double> a(action_count), b(action_count);
        single.average_strategy_into(index, action_count, a);
        repeat.average_strategy_into(index, action_count, b);
        for (std::size_t i = 0; i < action_count; ++i) {
            REQUIRE(a[i] == b[i]);
        }
    }
}

TEST_CASE("scrambled_seed gives distinct streams per thread index") {
    REQUIRE(cfr::solver::scrambled_seed(20260726, 0) == 20260726);
    std::set<std::uint64_t> seen;
    for (std::size_t index = 0; index < 16; ++index) {
        seen.insert(cfr::solver::scrambled_seed(20260726, index));
    }
    REQUIRE(seen.size() == 16);
}

TEST_CASE("load_strategy_sums rejects a checkpoint of the wrong size") {
    RhodeIslandGame game;
    REQUIRE(game.infoset_count() == 3142152);
    ExternalSamplingSolver<RhodeIslandGame> solver(game, 1);

    const std::filesystem::path path = std::filesystem::temp_directory_path() /
                                       "cfr-mccfr-checkpoint-wrong-size.bin";
    struct RemoveFile {
        std::filesystem::path path;
        ~RemoveFile() { std::filesystem::remove(path); }
    } cleanup{path};

    {
        std::ofstream output(path, std::ios::binary);
        // One value short of the table load_strategy_sums requires.
        std::uint64_t value_count =
            static_cast<std::uint64_t>(game.infoset_count()) *
                RhodeIslandGame::kMaxActions -
            1;
        int iteration = 1;
        output.write(reinterpret_cast<const char*>(&value_count),
                     sizeof(value_count));
        output.write(reinterpret_cast<const char*>(&iteration),
                     sizeof(iteration));
        REQUIRE(output.good());
    }

    REQUIRE_THROWS_WITH(
        solver.load_strategy_sums(path.string()),
        Catch::Matchers::ContainsSubstring("strategy table size mismatch"));
}
