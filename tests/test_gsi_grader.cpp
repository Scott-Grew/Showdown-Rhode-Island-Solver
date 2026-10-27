// The Rhode Island best-response walk, checked on fold payoffs, value
// conservation and the published equilibrium. The equilibrium tests
// pass without checking anything when the GSI.jar data files are
// absent.

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include <vector>

#include "game/rhode_island.hpp"
#include "grader/gsi_strategy.hpp"
#include "solver/rih_best_response.hpp"

using Catch::Matchers::WithinAbs;
using cfr::grader::GsiStrategy;
using namespace cfr::game;

namespace {

cfr::solver::RihStrategyQuery player0_bets_only_in_round(int target_round) {
    return [target_round](const State& state, Player acting_player,
                          std::vector<double>& probabilities) {
        RhodeIslandGame game;
        ParsedRounds parsed = parse_rounds(state);
        std::span<const Action> round_actions = active_round_actions(parsed);
        std::size_t action_count = game.legal_actions(state).size();
        probabilities.assign(
            static_cast<std::size_t>(cfr::kCardCount) * action_count, 0.0);

        bool facing_bet =
            !round_actions.empty() && round_actions.back() == kActionRaise;
        std::size_t choice = !facing_bet && acting_player == 0 &&
                                     parsed.board_cards_dealt == target_round
                                 ? 1
                                 : 0;
        for (int card = 0; card < cfr::kCardCount; ++card) {
            std::size_t card_index = static_cast<std::size_t>(card);
            probabilities[card_index * action_count + choice] = 1.0;
        }
    };
}

}

TEST_CASE(
    "a fold before the board is dealt wins the opponent ante in each round") {
    for (int round = 0; round < 3; ++round) {
        cfr::solver::RihStrategyQuery profile =
            player0_bets_only_in_round(round);
        REQUIRE_THAT(cfr::solver::rih_strategy_value(profile, 0),
                     WithinAbs(kRihAnte, 1e-9));
    }
}

TEST_CASE("rhode island walk conserves value between the two players") {
    GsiStrategy strategy = GsiStrategy::load(CFR_GSI_DATA_DIR);
    INFO("GSI equilibrium data absent; run `make data/gsi/partition`");
    REQUIRE(strategy.available());
    cfr::solver::RihStrategyQuery query =
        cfr::grader::gsi_strategy_query(strategy);
    REQUIRE_THAT(cfr::solver::rih_strategy_value(query, 0) +
                     cfr::solver::rih_strategy_value(query, 1),
                 WithinAbs(0.0, 1e-9));
}

TEST_CASE(
    "published equilibrium is an exact best response in betting round one") {
    GsiStrategy strategy = GsiStrategy::load(CFR_GSI_DATA_DIR);
    INFO("GSI equilibrium data absent; run `make data/gsi/partition`");
    REQUIRE(strategy.available());
    cfr::solver::RihStrategyQuery query =
        cfr::grader::gsi_strategy_query(strategy);

    for (Player player = 0; player < kPlayerCount; ++player) {
        double follow = cfr::solver::rih_strategy_value(query, player);
        double deviate =
            cfr::solver::rih_best_response_value_in_round(query, player, 0);
        REQUIRE_THAT(deviate - follow, WithinAbs(0.0, 1e-5));
    }
}

// Pins the number cfr_measure --published prints. A change in the
// best-response walk moves this value.
TEST_CASE("published equilibrium exploitability is the measured value") {
    GsiStrategy strategy = GsiStrategy::load(CFR_GSI_DATA_DIR);
    INFO("GSI equilibrium data absent; run `make data/gsi/partition`");
    REQUIRE(strategy.available());
    cfr::solver::RihExploitability report = cfr::solver::rih_exploitability(
        cfr::grader::gsi_strategy_query(strategy));
    REQUIRE_THAT(report.exploitability, WithinAbs(0.805328, 1e-6));
}
