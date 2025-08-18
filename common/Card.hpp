#include <string>
#include <nlohmann/json.hpp>
#pragma once
class Card {
public:

    unsigned id;
    std::string title;
    std::string effect;
    std::string type;
    int cmc;

    Card() = default;
    
    Card( const unsigned id_,
          const std::string& t, 
          const std::string& e, 
          const std::string & p,
          const unsigned cmc_
        )
        : id(id_), title(t), effect(e), type(p), cmc(cmc_){}

    // Optional: a method to display card info as a string
    std::string toString() const {
        return "Title: " + title + "\nEffect: ";
    }
};

inline void to_json(nlohmann::json& j, const Card& c) {
    j = nlohmann::json{{"title", c.title}, {"effect", c.effect}, {"id", c.id}, {"type", c.type}};
}

inline void from_json(const nlohmann::json& j, Card& c) {
    j.at("title").get_to(c.title);
    j.at("effect").get_to(c.effect);
    j.at("id").get_to(c.id);
    j.at("type").get_to(c.type);
}
