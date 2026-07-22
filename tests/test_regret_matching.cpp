#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>

#include <numeric>
#include <vector>

#include "solver/strategy.hpp"

using namespace cfr::solver;

TEST_CASE("regret matching: proportional to positive regrets") {   // V10
    auto strategy = regret_matching_strategy({3.0, 1.0, -2.0});
    REQUIRE(strategy.size() == 3);
    CHECK(strategy[0] == Catch::Approx(0.75));
    CHECK(strategy[1] == Catch::Approx(0.25));
    CHECK(strategy[2] == Catch::Approx(0.0));
}

TEST_CASE("regret matching: all non-positive -> uniform") {         // V10
    auto strategy = regret_matching_strategy({-1.0, 0.0, -5.0, 0.0});
    for (double p : strategy) CHECK(p == Catch::Approx(0.25));
}

TEST_CASE("regret matching: output sums to 1") {                    // V10
    // property over a handful of directed regret vectors incl. zeros/mixed
    std::vector<std::vector<double>> regret_vectors = {
        {5.0, 0.0, 0.0},
        {0.0, 0.0, 0.0},
        {-1.0, -2.0, -3.0},
        {2.0, -1.0, 4.0, 0.0},
        {0.0, 3.0},
        {4.0},
    };
    for (const auto& cumulative_regrets : regret_vectors) {
        auto strategy = regret_matching_strategy(cumulative_regrets);
        double probability_sum = std::accumulate(strategy.begin(), strategy.end(), 0.0);
        CHECK(probability_sum == Catch::Approx(1.0));
    }
}

TEST_CASE("regret matching+: floors cumulative regret at zero every update") {  // V16
    std::vector<double> cumulative_regrets = {2.0, -1.0};
    accumulate_regret_plus(cumulative_regrets, {-5.0, -0.5});
    CHECK(cumulative_regrets[0] == Catch::Approx(0.0));  // 2.0 - 5.0 = -3.0 -> floored
    CHECK(cumulative_regrets[1] == Catch::Approx(0.0));  // -1.0 - 0.5 = -1.5 -> floored

    accumulate_regret_plus(cumulative_regrets, {3.0, 0.2});
    CHECK(cumulative_regrets[0] == Catch::Approx(3.0));  // 0 + 3.0, no floor triggered
    CHECK(cumulative_regrets[1] == Catch::Approx(0.2));  // 0 + 0.2, no floor triggered
}
