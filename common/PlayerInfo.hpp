#pragma once
#include "Card.hpp"
#include <vector>
struct PlayerInfo {
    unsigned player_id; // 0 or 1 in two player game
    std::vector<Card> hand_cards;
};


