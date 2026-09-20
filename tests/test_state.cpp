// State stays trivially copyable, so the per-node copies the solvers
// make stay plain memory copies.

#include <catch2/catch_test_macros.hpp>
#include <type_traits>

#include "game/game.hpp"

TEST_CASE("State is trivially copyable POD", "[state]") {
    STATIC_REQUIRE(std::is_trivially_copyable_v<cfr::game::State>);
    STATIC_REQUIRE(std::is_trivially_destructible_v<cfr::game::State>);
    STATIC_REQUIRE(sizeof(cfr::game::State) <= 64);
}
