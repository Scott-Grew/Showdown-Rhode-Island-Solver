#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>

#include <numeric>
#include <vector>

#include "solver/strategy.hpp"

using namespace cfr::solver;

TEST_CASE("V10: regret matching proportional to positive regrets") {
    std::vector<double> cumulative_regrets = {3.0, 1.0, -2.0};
    auto strategy = regret_matching_strategy(cumulative_regrets);
    REQUIRE(strategy.size() == 3);
    CHECK(strategy[0] == Catch::Approx(0.75));
    CHECK(strategy[1] == Catch::Approx(0.25));
    CHECK(strategy[2] == Catch::Approx(0.0));
}

TEST_CASE("V10: regret matching all non-positive -> uniform") {
    std::vector<double> cumulative_regrets = {-1.0, 0.0, -5.0, 0.0};
    auto strategy = regret_matching_strategy(cumulative_regrets);
    for (double p : strategy) CHECK(p == Catch::Approx(0.25));
}

TEST_CASE("V10: regret matching output sums to 1") {

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

TEST_CASE("V16: regret matching+ floors cumulative regret at zero every update") {
    std::vector<double> cumulative_regrets = {2.0, -1.0};
    accumulate_regret_plus(cumulative_regrets, std::vector<double>{-5.0, -0.5});
    CHECK(cumulative_regrets[0] == Catch::Approx(0.0));
    CHECK(cumulative_regrets[1] == Catch::Approx(0.0));

    accumulate_regret_plus(cumulative_regrets, std::vector<double>{3.0, 0.2});
    CHECK(cumulative_regrets[0] == Catch::Approx(3.0));
    CHECK(cumulative_regrets[1] == Catch::Approx(0.2));
}
