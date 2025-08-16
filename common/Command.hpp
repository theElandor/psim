#pragma once
#include <string>
#include <nlohmann/json.hpp>

enum class CommandCode {
    PlayCard, // Play a card
    PassPriority, // Pass priority to other player
    UploadDeck, // Command to upload deck
    Resign,
    Quit,
    Unknown
};


inline std::string commandCodeToString(CommandCode code) {
    switch (code) {
        case CommandCode::PlayCard:    return "Play Card";
        case CommandCode::PassPriority:return "Pass Priority";
        case CommandCode::UploadDeck:  return "Upload Deck";
        case CommandCode::Resign:      return "Resign"; 
        case CommandCode::Quit:        return "Quit"; 
        default:                       return "Unknown";
    }
}

inline CommandCode commandCodeFromString(const std::string& str) {
    if (str == "Play Card")     return CommandCode::PlayCard;
    if (str == "Pass Priority") return CommandCode::PassPriority;
    if (str == "Upload Deck")   return CommandCode::UploadDeck;
    if (str == "Resign")        return CommandCode::Resign;
    if (str == "Quit")          return CommandCode::Quit;
    return CommandCode::Unknown;
}

inline nlohmann::json serializeCommandCodeVector(const std::vector<CommandCode>& codes) {
    nlohmann::json j = nlohmann::json::array();
    for (const auto& code : codes) {
        j.push_back(commandCodeToString(code));
    }
    return j; // returns a JSON array
}

class Command {
public:
    Command() = default;

    Command(CommandCode code, const std::string& target="", const std::string& extra="")
        : code(code), target(target), extra(extra) {}

    std::string toString() const {
        return "Command(" + commandCodeToString(code) +
               ", target: " + target +
               ", extra: " + extra + ")";
    }

    CommandCode code{CommandCode::Unknown};
    std::string target; // e.g. card id, player id
    std::string extra;  // e.g. extra data (mana spent, zone, etc.)
};

inline void to_json(nlohmann::json& j, const Command& c) {
    j = nlohmann::json{
        {"code",   commandCodeToString(c.code)},
        {"target", c.target},
        {"extra",  c.extra}
    };
}

inline void from_json(const nlohmann::json& j, Command& c) {
    c.code   = commandCodeFromString(j.at("code").get<std::string>());
    c.target = j.at("target").get<std::string>();
    c.extra  = j.at("extra").get<std::string>();
}

