#include <iostream>
#include <string>
#include <vector>
#include "Card.hpp"
#include "Command.hpp"
// AI generated
std::string fitString(const std::string& str, size_t n) {
    if (str.size() > n) return str.substr(0, n-3) + "...";
    return str + std::string(n - str.size(), ' '); 
}
void print_commands(const std::vector<CommandCode> &codes){
  for(auto &p:codes){
    std::cout<<"-> "<<commandCodeToString(p)<<std::endl;
  }
}
void display_cards(const std::vector<Card>& hand) {
    const int width = 20;  // card width
 
    // Print top border of all cards
    for (size_t i = 0; i < hand.size(); ++i) {
        std::cout << "+" << std::string(width - 2, '-') << "+  ";
    }
    std::cout << "\n";

    // Print title line
    for (const auto& card : hand) {
        std::cout << "|" << fitString(card.title, width - 2) << "|  ";
    }
    std::cout << "\n";

    // Print separator line
    for (size_t i = 0; i < hand.size(); ++i) {
        std::cout << "|" << std::string(width - 2, '-') << "|  ";
    }
    std::cout << "\n";

    // Print effect line (trimmed)
    for (const auto& card : hand) {
        std::cout << "|" << fitString(card.effect, width - 2) << "|  ";
    }
    std::cout << "\n";

    // Empty line for spacing inside card
    for (size_t i = 0; i < hand.size(); ++i) {
        std::cout << "|" << std::string(width - 2, ' ') << "|  ";
    }
    std::cout << "\n";

    // Print bottom border of all cards
    for (size_t i = 0; i < hand.size(); ++i) {
        std::cout << "+" << std::string(width - 2, '-') << "+  ";
    }
    std::cout << "\n";
}
