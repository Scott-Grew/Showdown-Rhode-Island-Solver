// The State all games share and the GameLike concept the solvers
// are written against. Rhode Island is the product in src/; Kuhn and
// Leduc are test oracles in tests/oracles/.

#pragma once

#include <array>
#include <cassert>
#include <concepts>
#include <cstddef>
#include <cstdint>
#include <initializer_list>

namespace cfr::game {

using Player = std::size_t;
using Action = int;

inline constexpr Player kPlayerCount = 2;

constexpr Player opponent(Player player) {
    return kPlayerCount - 1 - player;
}

// Fixed capacities of the State arrays below. A betting round holds
// at most eight actions, and every game here deals from at most 52
// cards. History is 26 rather than 30 so each player's stored class
// fits and State stays within 64 bytes. The longest hand is 19
// entries.
inline constexpr std::size_t kMaxHistory = 26;
inline constexpr std::size_t kMaxBoardCards = 2;
inline constexpr std::size_t kMaxActionsPerRound = 8;
inline constexpr std::size_t kMaxChanceOutcomes = 52;

// Stack sequence with a compile-time cap. Traversal returns these
// instead of allocating a std::vector. Storage is not
// value-initialised: every element below size_ is written before
// it is read, and zeroing the tail showed up under the sampler.
template <typename T, std::size_t N>
class FixedVector {
public:
    FixedVector() = default;

    FixedVector(std::initializer_list<T> items) {
        assert(items.size() <= N);
        for (const T& item : items) push_back(item);
    }

    void push_back(const T& item) {
        assert(size_ < N);
        data_[size_++] = item;
    }

    std::size_t size() const { return size_; }
    bool empty() const { return size_ == 0; }
    T& operator[](std::size_t index) { return data_[index]; }
    const T& operator[](std::size_t index) const { return data_[index]; }
    T* begin() { return data_.data(); }
    const T* begin() const { return data_.data(); }
    T* end() { return data_.data() + size_; }
    const T* end() const { return data_.data() + size_; }

private:
    std::array<T, N> data_;
    std::size_t size_ = 0;
};

// One node of a game tree. A card entry of -1 means not dealt yet,
// and history holds chance and betting entries in play order.
//
// The fields after history are the betting round in progress.
// Leduc and Rhode Island update them in O(1) from apply_action;
// Kuhn leaves them at the defaults. The active round's actions
// are history's last round_len entries. contribution includes
// the ante. showdown_rank is filled when Rhode Island deals the
// turn. round_contribution and level_to_match are the running
// pot of the active round. closing[r] is the closing-sequence
// code of completed round r, written when that round's board
// card is dealt.
struct State {
    std::array<std::int8_t, 2> hole_cards{-1, -1};
    std::array<std::int8_t, kMaxBoardCards> board_cards{-1, -1};
    std::uint8_t board_count = 0;
    std::array<std::uint8_t, kMaxHistory> history{};
    std::uint8_t history_len = 0;
    std::uint8_t round_len = 0;
    std::uint8_t raises_used = 0;
    std::array<std::uint8_t, 2> closing{};
    std::array<int, 2> contribution{};
    std::array<int, 2> round_contribution{};
    int level_to_match = 0;
    std::array<std::uint16_t, 2> showdown_rank{};
    // card_class[p] is player p's refined class of the cards dealt
    // so far. Rhode Island writes it when a card is dealt and
    // infoset_index reads it. Other games leave it at zero.
    std::array<std::uint16_t, 2> card_class{};
};

static_assert(sizeof(State) <= 64, "State must stay a cheap copy");

struct ChanceOutcome {
    Action action;
    double probability;
};

// What a solver needs from a game. infoset_index must be below
// infoset_count, and equal for states the acting player cannot
// tell apart.
template <typename CandidateGame>
concept GameLike = requires(const CandidateGame game, const State state,
                            Action action, Player player) {
    { game.initial_state() } -> std::same_as<State>;
    { game.is_terminal(state) } -> std::same_as<bool>;
    { game.is_chance(state) } -> std::same_as<bool>;
    { game.current_player(state) } -> std::same_as<Player>;
    {
        game.legal_actions(state)
    } -> std::same_as<FixedVector<Action, CandidateGame::kMaxActions>>;
    { game.apply_action(state, action) } -> std::same_as<State>;
    { game.terminal_utility(state, player) } -> std::same_as<double>;
    { game.infoset_index(state) } -> std::same_as<std::uint32_t>;
    { game.infoset_count() } -> std::same_as<std::uint32_t>;
    {
        game.chance_outcomes(state)
    } -> std::same_as<FixedVector<ChanceOutcome, kMaxChanceOutcomes>>;
    { CandidateGame::kMaxActions } -> std::convertible_to<std::size_t>;
};

}
