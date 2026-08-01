#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <string>
#include <utility>
#include <vector>

namespace cfr::game {

using Player = int;
using Action = int;
using InfoSetKey = std::string;

inline constexpr std::size_t kMaxHistory = 32;
inline constexpr std::size_t kMaxPublic = 2;

struct State {
    std::array<std::int8_t, 2> private_cards{-1, -1};
    std::array<std::int8_t, kMaxPublic> public_cards{-1, -1};
    std::uint8_t public_count = 0;
    std::array<std::uint8_t, kMaxHistory> history{};
    std::uint8_t history_len = 0;
};

}
