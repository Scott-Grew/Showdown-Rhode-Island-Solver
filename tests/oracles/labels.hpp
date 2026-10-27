// Text infoset labels. The product keys on infoset_index; tests key a
// profile or a best response on the acting player's own cards.

#pragma once

#include <map>
#include <string>
#include <vector>

#include "game/game.hpp"

namespace cfr::game {

using InfosetLabel = std::string;

// A game that can name each infoset as text.
template <typename CandidateGame>
concept LabelledGame =
    GameLike<CandidateGame> && requires(const CandidateGame game) {
        { game.infoset_label(State{}) } -> std::same_as<InfosetLabel>;
    };

// Codes 0..max_raises opened with a check; a bet-open line sits above.
inline bool closing_opened_with_check(int closing, int max_raises) {
    return closing <= max_raises;
}

inline int closing_raises(int closing, int max_raises) {
    return closing_opened_with_check(closing, max_raises)
               ? closing
               : closing - max_raises;
}

// One action list exists for a raise count, an opening check, and
// whether the round closed, so a label does not rescan history.
// Check and call both print as "call"; bet and raise both as "raise".
inline void append_round_actions(std::string& label, int raises,
                                 bool opened_with_check, bool closed) {
    if (opened_with_check) label += "call,";
    for (int i = 0; i < raises; ++i) label += "raise,";
    if (closed) label += "call,";
}

}

namespace cfr::solver {

// Action probabilities per infoset label, in legal_actions order.
using StrategyProfile = std::map<game::InfosetLabel, std::vector<double>>;

}
