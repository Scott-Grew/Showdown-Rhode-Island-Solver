#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>

#include <vector>

#include "solver/strategy.hpp"

using namespace cfr::solver;

TEST_CASE("regret matching matches the closed form") {
    struct Row {
        std::vector<double> regrets;
        std::vector<double> expected;
    };
    Row rows[] = {
        {{3.0, 1.0, -2.0}, {0.75, 0.25, 0.0}},
        {{-1.0, 0.0, -5.0, 0.0}, {0.25, 0.25, 0.25, 0.25}},
        {{0.0, 0.0, 0.0}, {1.0 / 3.0, 1.0 / 3.0, 1.0 / 3.0}},
        {{-1.0, -2.0, -3.0}, {1.0 / 3.0, 1.0 / 3.0, 1.0 / 3.0}},
        {{2.0, -1.0, 4.0, 0.0}, {1.0 / 3.0, 0.0, 2.0 / 3.0, 0.0}},
        {{4.0}, {1.0}},
    };
    for (const Row& row : rows) {
        std::vector<double> strategy = row.regrets;
        regret_matching_in_place(strategy);
        REQUIRE(strategy.size() == row.expected.size());
        for (std::size_t i = 0; i < strategy.size(); ++i)
            CHECK(strategy[i] == Catch::Approx(row.expected[i]));
    }
}
