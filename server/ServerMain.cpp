#include <iostream>
#include <string>
#include <vector>
#include <memory>
#include <cstring>
#include <array>
#include <sstream>
#include <boost/asio.hpp>
#include <nlohmann/json.hpp>
#include "PublicInfo.hpp"
#include "Card.hpp"
#include "PlayerInfo.hpp"
#include "Command.hpp"
#include "Messages.hpp"

#define PORT 5000

using boost::asio::ip::tcp;
using json = nlohmann::json;

class Player {
public:
  int id;
  tcp::socket socket;
  PlayerInfo info;
  bool connected;
  bool ready; // deck has been uploaded and parsed.
  std::vector<char> read_buffer;
  uint32_t expected_message_length;
  bool reading_header;
  // private server information
  std::vector<Card> deck; 
  std::vector<Card> sideboard;
  Player(boost::asio::io_context& io, int player_id) 
      : id(player_id), socket(io), connected(false), expected_message_length(0), reading_header(true) {
      read_buffer.resize(65536); // 64KB buffer
  }
  void print_raw_deck(){
    std::cout<<"Main deck"<<"("<<deck.size()<<"):\n";
    for(auto &c:deck){
      std::cout<<c.title<<std::endl;
    }
    std::cout<<"Sideboard"<<"("<<sideboard.size()<<"):\n";
    for(auto &c:sideboard){
      std::cout<<c.title<<std::endl;
    }
  }
};

class GameServer {
private:
  boost::asio::io_context& io_context;
  tcp::acceptor acceptor;
  std::vector<std::shared_ptr<Player>> players;
  PublicInfo info;
  int connected_players;
  bool game_started;
    
public:
  GameServer(boost::asio::io_context& io) 
    : io_context(io), acceptor(io, tcp::endpoint(tcp::v4(), PORT)), 
      connected_players(0), game_started(false) {
    info.turn = 0;
    info.priority = 0;
    info.card_id = 0;
    info.life_points = {20, 20};
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
  /*
    &GameServer -> function pointer
    Need to bind function pointer to the current game server instance, 
    we do that with std::bind and pass also the pointer with "this".
   */

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
    // Blocking operation: deck upload.
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
  // different cards are initialized for each player.
  Card Forest_0("Forest", "", 0);
  Card Mountain_0("Mountain", "",1);
  Card LlanowarElves_0("Llanowar elves", "Tap to add 1 green", 2);

  Card Forest_1("Forest", "", 3);
  Card Mountain_1("Mountain", "",4);
  Card LlanowarElves_1("Llanowar elves", "Tap to add 1 green", 5);

  if (player.id == 0) {
      player.info = {0, {Forest_0, Mountain_0, Forest_0}};
  } else {
      player.info = {1, {LlanowarElves_1, Forest_1}};
  }
}

void start_game() {
// Broadcasts starting information to all players.
  if (game_started) return;
  game_started = true; 
  std::cout << "Game starting!\n";
  broadcast_public_info();
  for (auto& player : players) {
      send_player_info(player);
  }
  notify_priority_player();
}

void start_read(std::shared_ptr<Player> player) {
// If the player is ready, starts reading commands.
// Otherwise, will read a deck upload.
  if (!player->connected) return;
  else if (player->reading_header) {
    // Read message length (4 bytes)
    // handle_read_header is the callback.
    // _1 and _2 are passed from asio: the error code
    // and the size_t.
    // reads 4 bytes(sizeof uint32_t) into the address
    // of the player's expected_message_length.
    boost::asio::async_read(
      player->socket,
      boost::asio::buffer(&player->expected_message_length, sizeof(uint32_t)),
      std::bind(&GameServer::handle_read_header, this, player, std::placeholders::_1, std::placeholders::_2)
    );
  }
  else {
      // Read the actual message
      // reads expected_message_length bytes, previously read thanks
      // to the header.
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
  // Just interprets the command and starts reading again.
  if (!ec) {
    std::string json_str(
        player->read_buffer.begin(),
        player->read_buffer.begin() + player->expected_message_length
    );
    
    try {
        // Deserialize the Command from JSON
        nlohmann::json j = nlohmann::json::parse(json_str);
        Command command;
        from_json(j, command);        
        handle_command(player, command);
    } catch (const nlohmann::json::parse_error& e) {
        std::cerr << "JSON parse error " << player->id << ": " << e.what() << "\n";
        send_message(player, "Invalid command format");
    }
    
    // Reset for next message
    player->reading_header = true;
    start_read(player);
  } else {
      handle_disconnect(player, ec);
  }
}

void handle_disconnect(std::shared_ptr<Player> player, boost::system::error_code ec) {
// simple handling of disconnection of a player.
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
    
void handle_command(std::shared_ptr<Player> player, const Command &command) {
    std::cout << "Received command from player " << player->id << ": " << command.toString()<< "\n";
    if (command.code == CommandCode::Quit || command.code == CommandCode::Resign) {
        handle_player_resignation(player);
        return;
    }
    // For other commands, check if it's this player's priority
    if (player->id != info.priority) {
        send_message(player, MESSAGE_no_priority);
        return;
    }
    // Process the command for the player with priority
    std::cout<<MESSAGE_processing_command;
    std::string response = process_game_command(player, command);
    send_message(player, response);
    // Update game state and notify all players if needed
    broadcast_public_info();
    notify_priority_player();
}

bool parse_deck(std::shared_ptr<Player> player, const Command& command){
  /*
    This function parses a deck in the standard MTGO format.
    Probably needs extra refinment and security checks.
    AI kinds of writes very convoluted code, so I wrote this myself.
  */
  std::cout<<player->id<<" has uploaded a deck: \n";
  std::cout<<command.target<<std::endl;
  player->deck.clear();
  // turn string into vector of cards and assign it to player.
  std::cout<<"parse_deck -> starting deck_parsing..."<<std::endl;
  std::stringstream is(command.target);
  std::string line;
  int copies;
  bool sideboard = false;
  std::string name;
  while(true){
    if(!std::getline(is,line)){
      std::cout<<"Reached end of list.\n";
      player->print_raw_deck();
      return true;
    }
    std::stringstream is_line(line);
    is_line>>copies;
    if(!std::getline(is_line,name)){
      std::cout<<"Something went wrong during parsing.\n";
      return false;
    }
    for(int i = 0; i < copies; i++){
      // add card to either sideboard or main deck
      if(sideboard)
        player->sideboard.emplace_back(name, "", info.card_id++);
      else
        player->deck.emplace_back(name, "", info.card_id++);
    }
    if((int)is.peek() == 13){
      sideboard = true;
      is.ignore(2); // ignore carriage return and newline.
    }
  } 
  return false;
} 

bool starts_with(const std::string& str, const std::string& prefix) {
    return str.size() >= prefix.size() &&
           str.compare(0, prefix.size(), prefix) == 0;
}

std::string process_game_command(std::shared_ptr<Player> player, const Command &command) {
    // This is where you'd implement your actual game logic
    // The string is returned and sent to the client for now.
    if(!player->ready){ // if not ready
      if(command.code != CommandCode::UploadDeck){return MESSAGE_upload;}
      else{ // received an upload.
        if(parse_deck(player, command)){
          player->ready = true;
          return MESSAGE_correct_deck_upload;
        }
      }
    }
    else{ // if ready
      if (command.code == CommandCode::PassPriority) {
          // Switch priority to other player
          info.priority = (info.priority + 1) % 2;
          return MESSAGE_pass_priority;
      }
      else{return MESSAGE_unknown_command;}
    } 
  return MESSAGE_unknown_command;
}
    
void handle_player_resignation(std::shared_ptr<Player> player) {
  // Just disconnects every player.
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
  // Sends an asyncronous message to target player.

  if (!player->connected) return;
  // create shared pointer to message to avoid deallocating it      
  auto msg_copy = std::make_shared<std::string>(message);
  // get the length of the message and use htonl to make
  // it tranasferable through network.
  uint32_t len = htonl(static_cast<uint32_t>(message.size()));
  // creates a buffer to store the lenght 
  auto len_buffer = std::make_shared<std::array<char, sizeof(uint32_t)>>();
  // copy the length into the buffer.
  std::memcpy(len_buffer->data(), &len, sizeof(uint32_t));
  
  // create a vector of buffers to send together. 
  std::vector<boost::asio::const_buffer> buffers;
  // put in the buffer the length buffer and the message buffer.
  buffers.push_back(boost::asio::buffer(*len_buffer));
  buffers.push_back(boost::asio::buffer(*msg_copy));
  // send the buffers and the lambda is the callback,
  // so just a function executed when the operation is finished. 
  // Inside the callback you can use the captured parameters.
  boost::asio::async_write(player->socket, buffers,
    [this, player, msg_copy, len_buffer](boost::system::error_code ec, std::size_t length) {
      if (ec) { // if error during sending...
        handle_disconnect(player, ec);
      }
    });
}

void broadcast_message(const std::string& message) {
  // Sends a string to all players.
  for (auto& player : players) {
    if (player->connected) {
        send_message(player, message);
    }
  }
}
    
void send_player_info(std::shared_ptr<Player> player) {
  // Sends private information to target player. 
  nlohmann::json j;
  j["player_id"] = player->info.player_id;
  j["hand_cards"] = player->info.hand_cards;
  send_message(player, j.dump());
}

void send_public_info(std::shared_ptr<Player> player) { 
  // Sends information about game state to target player.
  nlohmann::json j;
  j["turn"] = info.turn;
  j["priority"] = info.priority;
  j["life_points"] =  info.life_points;
  send_message(player, j.dump());
}
void broadcast_public_info() {
  // Sends game state to all players.  
  for (auto& player : players) {
      if (player->connected) {
          send_public_info(player);
      }
  }
}

void send_available_commands(std::shared_ptr<Player> player) { 
  // Sends available commands to target player. Needed for frontend.
  std::vector<CommandCode> commands;
  if (player->ready){
    commands = {CommandCode::PlayCard, 
    CommandCode::PassPriority};
  }
  else{
    commands = {CommandCode::UploadDeck};
  }
  nlohmann::json j = serializeCommandCodeVector(commands);
  send_message(player, j.dump());
}

void notify_priority_player() { 
  // Sends a priority acquired notification to the player with priority.
  if (info.priority < players.size() && players[info.priority]->connected) {
      send_available_commands(players[info.priority]);
      send_message(players[info.priority], "You have priority");
      std::cout << "Player " << info.priority << " now has priority.\n";
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
