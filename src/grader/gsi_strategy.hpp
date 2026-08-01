#pragma once

#include <array>
#include <cstdint>
#include <string>
#include <unordered_map>
#include <vector>

#include "game/game.hpp"

namespace cfr::grader {

inline constexpr int kGsiSignalNodeCount = 135304;
inline constexpr int kGsiRound1ClassCount = 13;
inline constexpr int kGsiRound2ClassCount = 205;
inline constexpr int kGsiRound3ClassCount = 1774;
inline constexpr int kGsiSequencesPerRound = 7;
inline constexpr int kGsiSlotsPerKey = 10;

class GsiStrategy {
public:
    static GsiStrategy load(const std::string& directory);

    bool available() const { return available_; }

    std::vector<double> action_probabilities(const game::State& state) const;

    void action_probabilities_by_card(const game::State& state, game::Player actor,
                                       std::vector<double>& probabilities_by_card) const;

private:
    struct PublicContext;

    static PublicContext describe(const game::State& state);
    std::uint32_t key_for(const PublicContext& context, int hole) const;
    void write_slots(game::Player actor, std::uint32_t key, int type, double* destination) const;

    bool available_ = false;
    std::vector<std::uint16_t> signal_class_;
    std::array<std::unordered_map<std::uint32_t, std::array<float, kGsiSlotsPerKey>>, 2> behavior_;
};

}
