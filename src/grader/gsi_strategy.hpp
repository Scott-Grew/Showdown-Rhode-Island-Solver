// Reader for the Rhode Island equilibrium that Gilpin and Sandholm
// published as GSI.jar; the Gsi prefix refers to that file. It maps
// our states to their file keys so one best-response walk can grade
// their strategy and ours.

#pragma once

#include <array>
#include <cstdint>
#include <span>
#include <string>
#include <unordered_map>
#include <vector>

#include "game/game.hpp"

namespace cfr::grader {

// Dimensions of the published Gilpin-Sandholm strategy files:
// signal-tree size, classes per round, sequences and row width.
inline constexpr int kGsiSignalNodeCount = 135304;
inline constexpr int kGsiRound1ClassCount = 13;
inline constexpr int kGsiRound2ClassCount = 205;
inline constexpr int kGsiSequencesPerRound = 7;
inline constexpr int kGsiSlotsPerKey = 10;

// The published Gilpin-Sandholm Rhode Island equilibrium, read
// from its partition and player files.
class GsiStrategy {
public:
    // Reads partition, player1 and player2 from directory. A missing
    // or short file yields an object whose available() is false.
    static GsiStrategy load(const std::string& directory);

    // True when every file loaded completely.
    bool available() const { return available_; }

    // Strategy of the acting player at
    // state, one entry per legal action.
    std::vector<double> action_probabilities(const game::State& state) const;

    // Strategy of acting_player at state for every hole card, laid
    // out as card * action_count + action. Board cards stay zero.
    void action_probabilities_by_card(
        const game::State& state, game::Player acting_player,
        std::vector<double>& probabilities_by_card) const;

private:
    // What both players can see at a state, in GSI numbering.
    struct PublicContext;

    // Extracts board cards, betting sequences and the acting player.
    static PublicContext public_context(const game::State& state);
    // Row key in a player file for this hole card and context.
    std::uint32_t key_for(const PublicContext& context, int hole) const;
    // Writes the normalized probabilities of one decision to
    // destination, uniform when the key is absent or all zero.
    void write_slots(game::Player acting_player, std::uint32_t key,
                     int decision_type, std::span<double> destination) const;

    bool available_ = false;
    std::vector<std::uint16_t> signal_class_;
    std::array<
        std::unordered_map<std::uint32_t, std::array<float, kGsiSlotsPerKey>>,
        2>
        strategy_rows_;
};

}
