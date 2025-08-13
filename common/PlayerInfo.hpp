#pragma once
#include "Card.hpp"
#include <vector>
struct PlayerInfo {
    int player_id;
    std::vector<Card> hand_cards;
};


