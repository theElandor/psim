#include <iostream>
#include <string>
#include <vector>
#include <boost/asio.hpp>
#include "json.hpp"
#include "GameState.hpp" 
#include "Card.hpp"
#include "PlayerInfo.hpp"
#include "sprites.hpp"
#include "command.hpp"

using boost::asio::ip::tcp;
using json = nlohmann::json;


PlayerInfo deserializePlayerInfo(const std::string& json_str) {
    json j = json::parse(json_str);

    PlayerInfo p;
    p.player_id = j["player_id"];
    p.hand_cards = j["hand_cards"].get<std::vector<Card>>();
    return p;
}
GameState deserializeGameState(const std::string& json_str){
    json j = json::parse(json_str);

    GameState g;
    g.turn = j["turn"];
    g.priority = j["priority"];
    g.life_points = j["life_points"].get<std::pair<int,int>>(); 
    return g;
}
vector<CommandCode> deserializeAvailableCodes(const std::string &json_str){
    json j = json::parse(json_str);
    vector<CommandCode> commands;
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

std::string receiveMessage(tcp::socket& socket) {
    uint32_t len_net;
    boost::asio::read(socket, boost::asio::buffer(&len_net, sizeof(len_net)));
    uint32_t len = ntohl(len_net);

    std::vector<char> buf(len);
    boost::asio::read(socket, boost::asio::buffer(buf.data(), len));

    return std::string(buf.begin(), buf.end());
}

void sendCommand(tcp::socket& socket, const std::string& command) {
    uint32_t len = htonl(static_cast<uint32_t>(command.size()));
    std::vector<boost::asio::const_buffer> buffers;
    buffers.push_back(boost::asio::buffer(&len, sizeof(len)));
    buffers.push_back(boost::asio::buffer(command));
    boost::asio::write(socket, buffers);
}

PlayerInfo receivePlayerInfo(tcp::socket& socket) {
    uint32_t len_net;
    boost::asio::read(socket, boost::asio::buffer(&len_net, sizeof(len_net)));
    uint32_t len = ntohl(len_net);

    std::vector<char> buf(len);
    boost::asio::read(socket, boost::asio::buffer(buf.data(), len));

    std::string json_str(buf.begin(), buf.end());
    return deserializePlayerInfo(json_str);
}
GameState receiveGameState(tcp::socket &socket){
    uint32_t len_net;
    boost::asio::read(socket, boost::asio::buffer(&len_net, sizeof(len_net)));
    uint32_t len = ntohl(len_net);

    std::vector<char> buf(len);
    boost::asio::read(socket, boost::asio::buffer(buf.data(), len));

    std::string json_str(buf.begin(), buf.end());
    return deserializeGameState(json_str);
}
vector<CommandCode> receiveCommandCode(tcp::socket &socket){
    
}
int main() {
    try {
        boost::asio::io_context io;
        tcp::socket socket(io);

        socket.connect(tcp::endpoint(boost::asio::ip::make_address("127.0.0.1"), 5000));
        std::cout << "Connected to server.\n";
        PlayerInfo p = receivePlayerInfo(socket);
        GameState g = receiveGameState(socket);

        std::cout << "\n--- Private Information ---\n";
        std::cout << "Player ID: " << p.player_id << "\n";
        display_cards(p.hand_cards);

        std::cout << "\n--- Global Information ---\n";
        std::cout << "Life points: " << g.life_points.first <<" " <<g.life_points.second<< "\n";
        std::cout<< "Turn: "<<g.turn<<std::endl; 
        std::cout<< "Priority: "<<g.turn<<std::endl; 
        std::string priorityMessage = receiveMessage(socket);
        std::cout << "\n" << priorityMessage << "\n";

        // Check if this player has priority
        if (p.player_id == g.priority) {
            std::cout << "\nYou can now enter commands (type 'quit' to exit):\n";
            
            // Main command loop
            while (true) {

                std::cout << "> ";
                std::string command;
                std::getline(std::cin, command);
                
                if (command.empty()) continue;
                
                // Send command to server
                sendCommand(socket, command);
                std::cout << "Command sent: " << command << "\n";
                
                if (command == "quit") {
                    std::cout << "Exiting game...\n";
                    break;
                }
                else{
                    std::cout<<"Server answer is: ";
                    std::string ans = receiveMessage(socket); 
                    std::cout<<ans<<"\n";
                }
            }
        } else {
            std::cout << "\nWaiting for the other player to make their move...\n";
            // TODO: Listen for game state updates when it's not your turn
        }
        std::cout << "\n";
    } catch (std::exception& e) {
        std::cerr << "Client exception: " << e.what() << "\n";
    }
}

