#include <string>
#pragma once
class Card {
public:
    std::string title;
    std::string effect;

    Card() = default;

    Card(const std::string& t, const std::string& e)
        : title(t), effect(e) {}

    // Optional: a method to display card info as a string
    std::string toString() const {
        return "Title: " + title + "\nEffect: " + effect;
    }
};
inline void to_json(nlohmann::json& j, const Card& c) {
    j = nlohmann::json{{"title", c.title}, {"effect", c.effect}};
}

inline void from_json(const nlohmann::json& j, Card& c) {
    j.at("title").get_to(c.title);
    j.at("effect").get_to(c.effect);
}
