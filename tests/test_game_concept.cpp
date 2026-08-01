#include <catch2/catch_test_macros.hpp>

#include "game/game_concept.hpp"
#include "game/kuhn.hpp"
#include "game/leduc.hpp"

#include "game/rhode_island.hpp"

static_assert(cfr::game::GameLike<cfr::game::KuhnGame>);
static_assert(cfr::game::GameLike<cfr::game::LeducGame>);
static_assert(cfr::game::GameLike<cfr::game::RhodeIslandGame>);
static_assert(cfr::game::LabelledGame<cfr::game::KuhnGame>);
static_assert(cfr::game::LabelledGame<cfr::game::LeducGame>);

TEST_CASE("game classes satisfy GameLike", "[concept]") {
    REQUIRE(cfr::game::GameLike<cfr::game::KuhnGame>);
    REQUIRE(cfr::game::GameLike<cfr::game::LeducGame>);
    REQUIRE(cfr::game::GameLike<cfr::game::RhodeIslandGame>);
}
