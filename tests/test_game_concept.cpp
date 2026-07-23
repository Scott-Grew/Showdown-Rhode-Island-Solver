#include <catch2/catch_test_macros.hpp>

#include "game/game_concept.hpp"
#include "game/kuhn.hpp"
#include "game/leduc.hpp"

static_assert(cfr::game::GameLike<cfr::game::KuhnGame>);
static_assert(cfr::game::GameLike<cfr::game::LeducGame>);

TEST_CASE("game classes satisfy GameLike", "[concept]") {
    REQUIRE(cfr::game::GameLike<cfr::game::KuhnGame>);
    REQUIRE(cfr::game::GameLike<cfr::game::LeducGame>);
}
