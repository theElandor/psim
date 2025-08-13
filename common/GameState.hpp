#pragma once
#include <string>
#include <vector>
#include <sstream>
#include <utility>

struct GameState {
    int turn;  // playerID
    int priority; // playerID
    std::pair<int, int> life_points; // both player life points 
};

