#include <iostream>
#include <iterator>
#include <string>
#include <vector>
#include <memory>
#include <thread>
#include <cstring>
#include <array>
#include <fstream>
#include <boost/asio.hpp>
#include <nlohmann/json.hpp>
#include "GameState.hpp" 
#include "PlayerInfo.hpp"
#include "sprites.hpp"
#include "Command.hpp"
#include "Deserializers.hpp"

using boost::asio::ip::tcp;
using json = nlohmann::json;

class GameClient {
private:
    boost::asio::io_context& io_context;
    tcp::socket socket;
    PlayerInfo player_info;
    GameState game_state;
    bool connected;
    std::vector<char> read_buffer;
    uint32_t expected_message_length;
    bool reading_header;
    bool has_priority;
    std::vector<CommandCode> available_commands;
    
public:
GameClient(boost::asio::io_context& io) 
    : io_context(io), socket(io), connected(false), 
      expected_message_length(0), reading_header(true), has_priority(false) {
    read_buffer.resize(65536); // 64KB buffer
}
 
void connect_to_server(const std::string& host, int port) {
    // just a symple async connection.
    auto endpoint = tcp::endpoint(boost::asio::ip::make_address(host), port); 
    socket.async_connect(endpoint,
        [this](boost::system::error_code ec) {
            if (!ec) {
                connected = true;
                std::cout << "Connected to server.\n";
                start_read();
            } else {
                std::cerr << "Connection failed: " << ec.message() << "\n";
            }
        });
}

void send_command(const Command& command) {
  if (!connected) {
    std::cout << "Not connected to server!\n";
    return;
  } 
  try {
    // Serialize command to JSON
    nlohmann::json j;
    to_json(j, command);
    std::string json_str = j.dump();
    
    auto msg_copy = std::make_shared<std::string>(json_str);
    uint32_t len = htonl(static_cast<uint32_t>(json_str.size()));
    
    auto len_buffer = std::make_shared<std::array<char, sizeof(uint32_t)>>();
    std::memcpy(len_buffer->data(), &len, sizeof(uint32_t));
    
    std::vector<boost::asio::const_buffer> buffers;
    buffers.push_back(boost::asio::buffer(*len_buffer));
    buffers.push_back(boost::asio::buffer(*msg_copy));
    
    boost::asio::async_write(socket, buffers,
      [this, msg_copy, len_buffer, command](boost::system::error_code ec, std::size_t length) {
          if (!ec) {
              std::cout << "Command sent: " << command.toString() << "\n";
          } else {
              std::cerr << "Send error: " << ec.message() << "\n";
              handle_disconnect();
          }
        });
  } catch (const std::exception& e) {
      std::cerr << "Error serializing command: " << e.what() << "\n";
  }
}

std::string open_deck(){
  std::cout << "Insert a valid deck path: ";
  std::string path;
  while (true) {
    std::getline(std::cin, path);
    std::ifstream is(path);
    if (!is) {
        std::cout<<"Invalid path. Try again: ";
        continue;
    }
    
    std::string contents((std::istreambuf_iterator<char>(is)),
                       std::istreambuf_iterator<char>());
    is.close();
    return contents;
  }
  return "something went wrong";
}

Command create_command_from_input(CommandCode code) {
  Command cmd;
  cmd.code = code;
  
  switch (code) {      
    case CommandCode::UploadDeck: {
        std::string contents = open_deck(); 
        cmd.target = contents;
        break;
    }   
    case CommandCode::PassPriority:
      break;            
    default:
      break;
  } 
    return cmd;
}

void start_input_loop() {
    // Run input loop in a separate thread
    std::thread input_thread([this]() {
      // command is just a string for now.
      std::string input;
      while (connected) {
        // Always show prompt if connected, but indicate priority status
        if (has_priority) {
            std::cout << "\n[You have priority.] > ";
        } else {
            std::cout << "\n[WAITING] (type 'quit' or 'resign' to leave) > ";
        } 
        if (std::getline(std::cin, input)) {
          if (!input.empty()) {
            // Allow quit/resign commands at any time
            if (input == "quit") {
                send_command(Command(CommandCode::Quit));
                break;
            }
            if (input == "resign"){
                send_command(Command(CommandCode::Resign));
                break;
            }
            // For other commands, check if player has priority
            else if (has_priority) {
                CommandCode code = commandCodeFromString(input); 
                Command command = create_command_from_input(code);
                send_command(command);
            } else {
              std::cout<<"You don't have priority."<<std::endl;
            }
          }
        } else {
            break; // EOF or error
        }
      }
  }); 
    input_thread.detach(); // Let it run independently
}
 
private:
void start_read() {
  // TODO: check if this can be factored out
  // and if we can make a single function for both
  // server and client.
  if (!connected) return;
  if (reading_header) {
      // Read message length (4 bytes)
    boost::asio::async_read(socket,
      boost::asio::buffer(&expected_message_length, sizeof(uint32_t)),
      [this](boost::system::error_code ec, std::size_t length) {
        if (!ec) {
            expected_message_length = ntohl(expected_message_length);
            reading_header = false;
            start_read(); // Read the actual message
        } else {
            handle_disconnect();
          }
        });
    } else {
      // Read the actual message
      boost::asio::async_read(socket,
        boost::asio::buffer(read_buffer.data(), expected_message_length),
        [this](boost::system::error_code ec, std::size_t length) {
            if (!ec) {
              std::string message(read_buffer.begin(), 
                                read_buffer.begin() + expected_message_length);
              handle_message(message); 
              // Reset for next message
              reading_header = true;
              start_read(); // Continue reading
            } else {
                handle_disconnect();
            }
          });
    }
}
    
void handle_message(const std::string& message) {
    /*
    * Main logic of message reception from the server.
    * Currently supporting the following messages:
    * 1) Player's private information (JSON)
    * 2) Shared global game state (JSON)
    * 3) Set of available commands (JSON)
    * 4) String messages (str)
    */    
    try {
        // Try to parse as JSON first
        nlohmann::json j = nlohmann::json::parse(message);
        
        // Check if it's player info
        if (j.contains("player_id") && j.contains("hand_cards")) {
          player_info = deserialize_player_info(message);
          display_player_info();
          return;
        }
        
        // Check if it's game state
        if (j.contains("turn") && j.contains("priority") && j.contains("life_points")) {
          game_state = deserialize_game_state(message);
          display_game_state();
          check_priority();
          return;
        }
        
        // Check if it's available commands
        if (j.is_array()) {
          available_commands = deserialize_available_codes(message);
          display_available_commands();
          return;
        }
        
    } catch (const nlohmann::json::parse_error&) {
        // Not JSON, treat as plain text message
    }
    
    // Handle plain text messages
    std::cout << "\n" << message << "\n";
    
    if (message == "You have priority") {
        has_priority = true;
        std::cout << "You can now enter commands!\n";
    } else if (message.find("has left the game") != std::string::npos ||
               message.find("has resigned") != std::string::npos ||
               message.find("Game over") != std::string::npos) {
        std::cout << "Game ended.\n";
        connected = false;
    }
}
    
void handle_disconnect() {
  // Handles disconnection of the current client.
  if (connected) {
    std::cout << "\nDisconnected from server.\n";
    connected = false;
    try {
        socket.close();
    } catch (...) {}
  }
}
            
void check_priority() {
  bool new_priority = (player_info.player_id == game_state.priority);
  if (new_priority != has_priority) {
      has_priority = new_priority;
      if (!has_priority) {
          std::cout << "\nWaiting...(you can still type 'quit' or 'resign')\n";
      }
  }
}

void display_player_info() {
  std::cout << "\n--- Private Information ---\n";
  std::cout << "Player ID: " << player_info.player_id << "\n";
  display_cards(player_info.hand_cards);
}
    
void display_game_state() {
  std::cout << "\n--- Global Information ---\n";
  std::cout << "Life points: " << game_state.life_points.first 
            << " " << game_state.life_points.second << "\n";
  std::cout << "Turn: " << game_state.turn << std::endl;
  std::cout << "Priority: " << game_state.priority << std::endl;
}

void display_available_commands() {
  std::cout << "\nAvailable commands:\n";
  print_commands(available_commands);
}
};
int main() {
    try {
        boost::asio::io_context io;
        GameClient client(io);
        
        // Connect to server
        client.connect_to_server("127.0.0.1", 5000);
        
        // Start the input loop for user commands
        client.start_input_loop();
        
        // Run the io_context to handle async operations
        io.run();
        
    } catch (std::exception& e) {
        std::cerr << "Client exception: " << e.what() << "\n";
    }
    
    return 0;
}
