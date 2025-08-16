#pragma once
#include <utility>
#include "Card.hpp"

struct PublicInfo {
    unsigned turn;  // playerID
    unsigned priority; // playerID
    unsigned card_id; // currently available card_id
    std::pair<int, int> life_points; // both player life points 
};

