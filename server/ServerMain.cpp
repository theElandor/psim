#include <iostream>
#include <string>
#include <vector>
#include <thread>
#include <boost/asio.hpp>
#include "json.hpp"
#include "GameState.hpp" 
#include "Card.hpp"
#include "PlayerInfo.hpp"
#include "Command.hpp"
#define PORT 5000

using boost::asio::ip::tcp;
using json = nlohmann::json;


std::string serializePlayerInfo(const PlayerInfo& p) {
    json j;
    j["player_id"] = p.player_id;
    j["hand_cards"] = p.hand_cards;
    return j.dump();
}

std::string serializeGameState(const GameState &g){
    json j;
    j["turn"] = g.turn;
    j["priority"] = g.priority;
    j["life_points"] = g.life_points;
    return j.dump();
}
void sendMessage(tcp::socket& socket, const std::string& message) {
    uint32_t len = htonl(static_cast<uint32_t>(message.size()));
    std::vector<boost::asio::const_buffer> buffers;
    buffers.push_back(boost::asio::buffer(&len, sizeof(len)));
    buffers.push_back(boost::asio::buffer(message));
    boost::asio::write(socket, buffers);
}

void sendPlayerInfo(tcp::socket& socket, const PlayerInfo& p) {
    std::string msg = serializePlayerInfo(p);
    sendMessage(socket, msg);
}

void sendGameState(tcp::socket& socket, const GameState& g) {
    std::string msg = serializeGameState(g);
    sendMessage(socket, msg);
}
void sendAvailableCommands(tcp::socket &socket){
    // available commands depend on the game state itself.
    // for now, play card and pass priority are available. 
    std::vector<CommandCode> commands {CommandCode::PlayCard, CommandCode::PassPriority};
    std::string msg = serializeCommandCodeVector(commands);
    sendMessage(socket, msg);
}
std::string receiveCommand(tcp::socket& socket) {
    uint32_t len;
    boost::asio::read(socket, boost::asio::buffer(&len, sizeof(len)));
    len = ntohl(len);
    
    std::vector<char> buffer(len);
    boost::asio::read(socket, boost::asio::buffer(buffer));
    
    return std::string(buffer.begin(), buffer.end());
}
std::string handle_command(std::string command, std::vector<tcp::socket> &players, GameState &G){
    return "nothing happens for now";
}
int main() {
    try {
        boost::asio::io_context io;

        tcp::acceptor acceptor(io, tcp::endpoint(tcp::v4(), PORT));
        std::cout << "Server started. Waiting for 2 players...\n";

        std::vector<tcp::socket> players;
        for (int i = 0; i < 2; ++i) {
            players.emplace_back(io);
            acceptor.accept(players.back());
            std::cout << "Player " << i << " connected.\n";
        }

        // Sample player infos
        std::vector<PlayerInfo> playerInfos(2);
        Card Forest("Forest", "");
        Card Mountain("Mountain", "");
        Card LlanowarElves("Llanowar elves", "Tap to add 1 green");
       
        playerInfos[0] = {0, {Forest, Mountain, Forest}};
        playerInfos[1] = {1, {LlanowarElves, Forest}};
        // Initial Game state
        GameState g;
        g.turn = 0; g.priority =0; g.life_points = {20,20};
        // Sending private information and game state to both players
        for (int i = 0; i < 2; ++i) {
            sendPlayerInfo(players[i], playerInfos[i]);
            sendGameState(players[i], g); 
        }
        sendMessage(players[g.priority], "You have priority");
        std::cout << "Player " << g.priority << " now has priority." << "\n"; 
        // Main server loop - wait for commands from the player with priority
        while (true) {
            try {
                // first, send available commands to player with priority.
                sendAvailableCommands(players[g.priority]);
                std::cout<<"Sent commands"<<std::endl;
                std::string command = receiveCommand(players[g.priority]);
                std::cout << "Received command from player " << g.priority << ": " << command << "\n";
                std::string ans = handle_command(command, players, g);
                std::cout<<ans<<std::endl;
                sendMessage(players[g.priority], ans);
                // Example: if command is "quit", break the loop
                if (command == "quit") {
                    std::cout << "Player " << g.priority << " quit the game.\n";
                    break;
                } 
                // TODO: Add game logic to process commands and potentially change priority
                
            } catch (std::exception& e) {
                std::cerr << "Error receiving command: " << e.what() << "\n";
                break;
            }
        }
    } catch (std::exception& e) {
        std::cerr << "Server exception: " << e.what() << "\n";
    }
}

