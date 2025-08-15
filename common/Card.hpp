#include <string>
#include <nlohmann/json.hpp>
#pragma once
class Card {
public:
    std::string title;
    std::string effect;
    unsigned id;

    Card() = default;
    
    Card(const std::string& t, const std::string& e, const unsigned id_)
        : title(t), effect(e), id(id_) {}

    // Optional: a method to display card info as a string
    std::string toString() const {
        return "Title: " + title + "\nEffect: " + effect;
    }
};

inline void to_json(nlohmann::json& j, const Card& c) {
    j = nlohmann::json{{"title", c.title}, {"effect", c.effect}, {"id", c.id}};
}

inline void from_json(const nlohmann::json& j, Card& c) {
    j.at("title").get_to(c.title);
    j.at("effect").get_to(c.effect);
    j.at("id").get_to(c.id);
}
