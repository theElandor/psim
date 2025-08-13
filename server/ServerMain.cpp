#include <iostream>
#include <string>
#include <vector>
#include <memory>
#include <unordered_map>
#include <cstring>
#include <array>
#include <boost/asio.hpp>
#include <nlohmann/json.hpp>
#include "GameState.hpp" 
#include "Card.hpp"
#include "PlayerInfo.hpp"
#include "Command.hpp"
#define PORT 5000

using boost::asio::ip::tcp;
using json = nlohmann::json;

class Player {
public:
    int id;
    tcp::socket socket;
    PlayerInfo info;
    bool connected;
    std::vector<char> read_buffer;
    uint32_t expected_message_length;
    bool reading_header;
    
    Player(boost::asio::io_context& io, int player_id) 
        : id(player_id), socket(io), connected(false), expected_message_length(0), reading_header(true) {
        read_buffer.resize(65536); // 64KB buffer
    }
};

class GameServer {
private:
    boost::asio::io_context& io_context;
    tcp::acceptor acceptor;
    std::vector<std::shared_ptr<Player>> players;
    GameState game_state;
    int connected_players;
    bool game_started;
    
public:
    GameServer(boost::asio::io_context& io) 
        : io_context(io), acceptor(io, tcp::endpoint(tcp::v4(), PORT)), 
          connected_players(0), game_started(false) {
        game_state.turn = 0;
        game_state.priority = 0;
        game_state.life_points = {20, 20};
    }
    
    void start() {
        std::cout << "Server started. Waiting for players...\n";
        accept_connections();
    }
    
private:

    void accept_connections() {
        /*
          Function called as the server starts.
          The acceptor waits for async connections.
          When a client connects, the connection is handled
          by the "handle_accept()" function, wrapped around the "async_accept()"
          method of the acceptor.
         */
        if (connected_players >= 2) return;
        auto new_player = std::make_shared<Player>(io_context, connected_players);
        acceptor.async_accept(
          new_player->socket,
          std::bind(&GameServer::handle_accept, this, new_player, std::placeholders::_1)
        );
    }

    void handle_accept(std::shared_ptr<Player> new_player,boost::system::error_code ec) {
      /*
        This function initializes some player specific information.
        Then if the game needs one more player, the "accept_connections()" loop
        is triggered again. Otherwise the game can start.
        This function also starts the loop that waits for that specific
        player commands.
       */
      if (!ec) {
        new_player->connected = true;
        players.push_back(new_player);
        connected_players++;
        std::cout << "Player " << new_player->id << " connected.\n";
        // Initialize player info
        initialize_player_info(*new_player);
        // Start reading from this player
        start_read(new_player);
        if (connected_players < 2) {
            accept_connections(); // Accept next player
        } else {
          start_game();
        }
      } else {
        std::cerr << "Accept error: " << ec.message() << "\n";
      }
    }
    
    void initialize_player_info(Player& player) {
      // Just initializes some player information.
        Card Forest("Forest", "");
        Card Mountain("Mountain", "");
        Card LlanowarElves("Llanowar elves", "Tap to add 1 green");
        
        if (player.id == 0) {
            player.info = {0, {Forest, Mountain, Forest}};
        } else {
            player.info = {1, {LlanowarElves, Forest}};
        }
    }
    
    void start_game() {
        /*
         * Function that broadcasts starting information
         * to all players.
         **/
        if (game_started) return;
        game_started = true; 
        std::cout << "Game starting!\n";
        broadcast_game_state();
        for (auto& player : players) {
            send_player_info(player);
        }
        notify_priority_player();
    }

    void start_read(std::shared_ptr<Player> player) {
        if (!player->connected) return;

        if (player->reading_header) {
            // Read message length (4 bytes)
            boost::asio::async_read(
                player->socket,
                boost::asio::buffer(&player->expected_message_length, sizeof(uint32_t)),
                std::bind(&GameServer::handle_read_header, this, player, std::placeholders::_1, std::placeholders::_2)
            );
        } else {
            // Read the actual message
            boost::asio::async_read(
                player->socket,
                boost::asio::buffer(player->read_buffer.data(), player->expected_message_length),
                std::bind(&GameServer::handle_read_body, this, player, std::placeholders::_1, std::placeholders::_2)
            );
        }
    }

    void handle_read_header(std::shared_ptr<Player> player, boost::system::error_code ec, std::size_t /*length*/) {
        if (!ec) {
            player->expected_message_length = ntohl(player->expected_message_length);
            player->reading_header = false;
            start_read(player); // Proceed to read the body
        } else {
            handle_disconnect(player, ec);
        }
    }

    void handle_read_body(std::shared_ptr<Player> player, boost::system::error_code ec, std::size_t /*length*/) {
        if (!ec) {
            std::string command(
                player->read_buffer.begin(),
                player->read_buffer.begin() + player->expected_message_length
            );
            handle_command(player, command);
            // Reset for next message
            player->reading_header = true;
            start_read(player); // Continue reading
        } else {
            handle_disconnect(player, ec);
        }
    }

    void handle_disconnect(std::shared_ptr<Player> player, boost::system::error_code ec) {
        if (player->connected) {
            std::cout << "Player " << player->id << " disconnected: " << ec.message() << "\n";
            player->connected = false;
            connected_players--;
            // Notify other players about disconnection
            broadcast_message("Player " + std::to_string(player->id) + " has left the game");
            // Handle game state changes due to disconnection
            handle_player_resignation(player);
        }
    }
    
    void handle_command(std::shared_ptr<Player> player, const std::string& command) {
        std::cout << "Received command from player " << player->id << ": " << command << "\n";
        // Allow quit/resign at any time, regardless of priority
        if (command == "quit" || command == "resign") {
            handle_player_resignation(player);
            return;
        }
        // For other commands, check if it's this player's priority
        if (player->id != game_state.priority) {
            send_message(player, "It's not your turn! You can only quit/resign when it's not your turn.");
            return;
        }
        // Process the command for the player with priority
        std::string response = process_game_command(player, command);
        send_message(player, response);
        // Update game state and notify all players if needed
        broadcast_game_state();
        notify_priority_player();
    }
    
    std::string process_game_command(std::shared_ptr<Player> player, const std::string& command) {
        // This is where you'd implement your actual game logic
        // For now, just a placeholder
        if (command == "pass") {
            // Switch priority to other player
            game_state.priority = (game_state.priority + 1) % 2;
            return "Priority passed";
        }
        return "Command processed: " + command;
    }
    
    void handle_player_resignation(std::shared_ptr<Player> player) {
        std::string message = "Player " + std::to_string(player->id) + " has resigned. Game over.";
        broadcast_message(message);
        // Close all connections and stop the server
        for (auto& p : players) {
            if (p->connected && p != player) {
                try {
                    p->socket.close();
                } catch (...) {}
                p->connected = false;
            }
        }
        std::cout << message << "\n";
    }
    
    void send_message(std::shared_ptr<Player> player, const std::string& message) {
        if (!player->connected) return;
        
        auto msg_copy = std::make_shared<std::string>(message);
        uint32_t len = htonl(static_cast<uint32_t>(message.size()));
        
        auto len_buffer = std::make_shared<std::array<char, sizeof(uint32_t)>>();
        std::memcpy(len_buffer->data(), &len, sizeof(uint32_t));
        
        std::vector<boost::asio::const_buffer> buffers;
        buffers.push_back(boost::asio::buffer(*len_buffer));
        buffers.push_back(boost::asio::buffer(*msg_copy));
        
        boost::asio::async_write(player->socket, buffers,
            [this, player, msg_copy, len_buffer](boost::system::error_code ec, std::size_t length) {
                if (ec) {
                    handle_disconnect(player, ec);
                }
            });
    }
    
    void broadcast_message(const std::string& message) {
        for (auto& player : players) {
            if (player->connected) {
                send_message(player, message);
            }
        }
    }
    
    void send_player_info(std::shared_ptr<Player> player) {
        nlohmann::json j;
        j["player_id"] = player->info.player_id;
        j["hand_cards"] = player->info.hand_cards;
        send_message(player, j.dump());
    }
    
    void send_game_state(std::shared_ptr<Player> player) {
        nlohmann::json j;
        j["turn"] = game_state.turn;
        j["priority"] = game_state.priority;
        j["life_points"] = game_state.life_points;
        send_message(player, j.dump());
    }
    
    void broadcast_game_state() {
        for (auto& player : players) {
            if (player->connected) {
                send_game_state(player);
            }
        }
    }
    
    void send_available_commands(std::shared_ptr<Player> player) {
        std::vector<CommandCode> commands {CommandCode::PlayCard, CommandCode::PassPriority};
        nlohmann::json j = serializeCommandCodeVector(commands);
        send_message(player, j.dump());
    }
    
    void notify_priority_player() {
        if (game_state.priority < players.size() && players[game_state.priority]->connected) {
            send_available_commands(players[game_state.priority]);
            send_message(players[game_state.priority], "You have priority");
            std::cout << "Player " << game_state.priority << " now has priority.\n";
        }
    }
};

int main() {
    try {
        boost::asio::io_context io;
        GameServer server(io);
        server.start();
        // Run the io_context
        io.run();
    } catch (std::exception& e) {
        std::cerr << "Server exception: " << e.what() << "\n";
    }
    return 0;
}
