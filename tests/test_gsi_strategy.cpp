#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include <vector>

#include "game/card.hpp"
#include "game/rhode_island.hpp"
#include "grader/gsi_strategy.hpp"

using Catch::Matchers::WithinAbs;
using cfr::make_card;
using cfr::grader::GsiStrategy;
using namespace cfr::game;

namespace {

State facing_opening_raise(int hole_rank, int hole_suit) {
    RhodeIslandGame game;
    State state = game.initial_state();
    state = game.apply_action(state, kChanceCardOffset + make_card(hole_rank, hole_suit));
    state = game.apply_action(state, kChanceCardOffset + make_card((hole_rank + 1) % cfr::kRankCount,
                                                                      (hole_suit + 1) % cfr::kSuitCount));
    state = game.apply_action(state, kActionCallCheck);
    return game.apply_action(state, kActionRaise);
}

constexpr double kGilpinSandholmRound1FoldFrequency[cfr::kRankCount] = {
    1.0,      0.996309531, 0.985427557, 0.951675343, 0.777605156, 0.576978827, 0.731294,
    0.563508, 0.171509,    0.0,         0.0,         0.0,         0.0};

}

TEST_CASE("gsi round-one fold frequencies reproduce the published equilibrium") {
    GsiStrategy strategy = GsiStrategy::load(CFR_GSI_DATA_DIR);
    if (!strategy.available()) {
        SUCCEED("GSI equilibrium data absent, see STATUS for the fetch command");
        return;
    }

    for (int rank = 0; rank < cfr::kRankCount; ++rank) {
        std::vector<double> probabilities = strategy.action_probabilities(facing_opening_raise(rank, 0));
        REQUIRE(probabilities.size() == 3);
        REQUIRE_THAT(probabilities[0], WithinAbs(kGilpinSandholmRound1FoldFrequency[rank], 1e-6));
    }
}

TEST_CASE("gsi round-one strategy is suit invariant") {
    GsiStrategy strategy = GsiStrategy::load(CFR_GSI_DATA_DIR);
    if (!strategy.available()) {
        SUCCEED("GSI equilibrium data absent, see STATUS for the fetch command");
        return;
    }

    for (int rank = 0; rank < cfr::kRankCount; ++rank) {
        std::vector<double> clubs = strategy.action_probabilities(facing_opening_raise(rank, 0));
        for (int suit = 1; suit < cfr::kSuitCount; ++suit) {
            std::vector<double> other = strategy.action_probabilities(facing_opening_raise(rank, suit));
            REQUIRE(other.size() == clubs.size());
            for (std::size_t i = 0; i < clubs.size(); ++i) {
                REQUIRE_THAT(other[i], WithinAbs(clubs[i], 1e-12));
            }
        }
    }
}
