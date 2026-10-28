// Published round-one fold frequencies, and suit invariance of that row.
// Requires the GSI.jar files (make data/gsi/partition).

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
    state = game.apply_action(
        state, kChanceCardOffset + make_card(hole_rank, hole_suit));
    state = game.apply_action(
        state,
        kChanceCardOffset + make_card((hole_rank + 1) % cfr::kRankCount,
                                      (hole_suit + 1) % cfr::kSuitCount));
    state = game.apply_action(state, kActionCallCheck);
    return game.apply_action(state, kActionRaise);
}

// Gilpin and Sandholm's published round-one fold frequencies; the
// values are taken from the file and are not monotone in rank.
constexpr double kGilpinSandholmRound1FoldFrequency[cfr::kRankCount] = {
    1.0,         0.996309531, 0.985427557, 0.951675343, 0.777605156,
    0.576978827, 0.731294,    0.563508,    0.171509,    0.0,
    0.0,         0.0,         0.0};

}

TEST_CASE("gsi round-one folds match the paper and ignore suit") {
    GsiStrategy strategy = GsiStrategy::load(CFR_GSI_DATA_DIR);

    for (int rank = 0; rank < cfr::kRankCount; ++rank) {
        std::vector<double> clubs =
            strategy.action_probabilities(facing_opening_raise(rank, 0));
        REQUIRE(clubs.size() == 3);
        REQUIRE_THAT(clubs[0],
                     WithinAbs(kGilpinSandholmRound1FoldFrequency[rank], 1e-6));
        for (int suit = 1; suit < cfr::kSuitCount; ++suit) {
            std::vector<double> other =
                strategy.action_probabilities(facing_opening_raise(rank, suit));
            REQUIRE(other.size() == clubs.size());
            for (std::size_t i = 0; i < clubs.size(); ++i)
                REQUIRE_THAT(other[i], WithinAbs(clubs[i], 1e-12));
        }
    }
}
