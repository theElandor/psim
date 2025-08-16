#pragma once
#include <string>
#include <vector>
#include <nlohmann/json.hpp>
#include "PublicInfo.hpp" 
#include "Card.hpp"
#include "PlayerInfo.hpp"
#include "Command.hpp"

/*
* This file includes helper functions for deserialization
* of Json messages.
*/

PlayerInfo deserialize_player_info(const std::string& json_str) {
  nlohmann::json j = nlohmann::json::parse(json_str);
  PlayerInfo p;
  p.player_id = j["player_id"];
  p.hand_cards = j["hand_cards"].get<std::vector<Card>>();
  return p;
}
    
PublicInfo deserialize_public_info(const std::string& json_str) {
  nlohmann::json j = nlohmann::json::parse(json_str);
  PublicInfo info;
  info.turn = j["turn"];
  info.priority = j["priority"];
  info.life_points = j["life_points"].get<std::pair<int,int>>();
  return info;
}

std::vector<CommandCode> deserialize_available_codes(const std::string& json_str) {
  nlohmann::json j = nlohmann::json::parse(json_str);
  std::vector<CommandCode> commands;
  commands.reserve(j.size());
  
  for (const auto& item : j) {
      if (item.is_string()) {
          commands.push_back(commandCodeFromString(item.get<std::string>()));
      } else {
          commands.push_back(CommandCode::Unknown);
      }
  }
  return commands;
}
