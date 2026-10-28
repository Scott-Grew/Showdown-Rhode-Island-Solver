// Rhode Island infoset text, built from the public State fields. It
// names the dealt cards, so it stays strictly finer than the index.

#pragma once

#include <cstdint>
#include <string>

#include "game/card.hpp"
#include "game/rhode_island.hpp"
#include "oracles/labels.hpp"

namespace cfr::game {

using cfr::rih_card_name;

// "P<player>:<hole>|<board>:<r1>;<r2>;<r3>". Names stay finer than the
// GameShrink index. Each round's text is fixed by its counters. The
// acting player is round_len mod 2, which current_player returns at a
// decision node. One round per street: hole, flop, turn.
inline InfosetLabel rih_infoset_label(const State& state) {
    Player acting_player =
        static_cast<std::size_t>(state.round_len) % kPlayerCount;
    std::string label = "P" + std::to_string(acting_player) + ":" +
                        rih_card_name(state.hole_cards[acting_player]);
    for (std::uint8_t index = 0; index < state.board_count; ++index)
        label += "|" + rih_card_name(state.board_cards[index]);
    label += ":";
    constexpr int kStreets = static_cast<int>(kMaxBoardCards) + 1;
    for (int round = 0; round < kStreets; ++round) {
        if (round > 0) label += ";";
        if (round < state.board_count) {
            int code = state.closing[static_cast<std::size_t>(round)];
            append_round_actions(
                label, closing_raises(code, kRihMaxRaisesPerRound),
                closing_opened_with_check(code, kRihMaxRaisesPerRound), true);
        } else if (round == state.board_count) {
            append_round_actions(label, state.raises_used,
                                 opened_with_check(state), false);
        }
    }
    return label;
}

}
