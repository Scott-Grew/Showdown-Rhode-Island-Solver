#include <catch2/catch_test_macros.hpp>

#include <array>
#include <cstdint>
#include <span>
#include <unordered_map>
#include <vector>

#include "abstraction/rih_gameshrink.hpp"
#include "game/card.hpp"
#include "grader/gsi_strategy.hpp"

using cfr::Card;
using cfr::kCardCount;
using cfr::abstraction::RihPartition;
using cfr::grader::GsiStrategy;

namespace {
void check(const GsiStrategy& strategy, std::size_t round) {
    static const RihPartition part = RihPartition::compute();
    std::vector<int> fwd(part.class_count(round), -1);
    std::unordered_map<std::uint16_t, std::uint16_t> rev;
    auto same = [&](std::uint16_t ours, std::uint16_t theirs) {
        if (fwd[ours] >= 0) REQUIRE(fwd[ours] == theirs);
        if (fwd[ours] < 0) fwd[ours] = theirs;
        auto [it, ok] = rev.emplace(theirs, ours);
        if (!ok) REQUIRE(it->second == ours);
    };
    std::array<Card, 3> c{};
    auto cards = [&](std::size_t n) {
        return std::span<const Card>(c.data(), n);
    };
    for (int h = 0; h < kCardCount; ++h) {
        c[0] = static_cast<Card>(h);
        if (round == 0)
            same(part.class_of(cards(1)), strategy.signal_class(cards(1)));
        for (int f = 0; f < kCardCount && round > 0; ++f) {
            if (f == h) continue;
            c[1] = static_cast<Card>(f);
            if (round == 1)
                same(part.class_of(cards(2)), strategy.signal_class(cards(2)));
            for (int t = 0; t < kCardCount && round > 1; ++t) {
                if (t == h || t == f) continue;
                c[2] = static_cast<Card>(t);
                same(part.class_of(cards(3)), strategy.signal_class(cards(3)));
            }
        }
    }
    for (int id : fwd) REQUIRE(id >= 0);
    REQUIRE(rev.size() == fwd.size());
}

}

TEST_CASE("gameshrink class counts match the published partition counts") {
    const RihPartition part = RihPartition::compute();
    REQUIRE(part.class_count(0) == 13);
    REQUIRE(part.class_count(1) == 205);
    REQUIRE(part.class_count(2) == 1774);
}

TEST_CASE("gameshrink partition is equivalent to the published partition") {
    GsiStrategy strategy = GsiStrategy::load(CFR_GSI_DATA_DIR);
    INFO("requires GSI data: run make data/gsi/partition");
    REQUIRE(strategy.available());
    check(strategy, 0);
    check(strategy, 1);
    check(strategy, 2);
}
