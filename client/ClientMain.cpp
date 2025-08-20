#include <iostream>
#include <iterator>
#include <string>
#include <vector>
#include <memory>
#include <thread>
#include <cstring>
#include <array>
#include <fstream>
#include <atomic>
#include <mutex>
#include <queue>
#include <SDL2/SDL.h>
#include <SDL2/SDL_ttf.h>
#include <boost/asio.hpp>
#include <nlohmann/json.hpp>
#include "PublicInfo.hpp" 
#include "PlayerInfo.hpp"
#include "sprites.hpp"
#include "Command.hpp"
#include "Deserializers.hpp"

using boost::asio::ip::tcp;
using json = nlohmann::json;

// UI Constants
const int WINDOW_WIDTH = 1000;
const int WINDOW_HEIGHT = 700;
const int CONSOLE_HEIGHT = 200;
const int INPUT_HEIGHT = 30;
const int MARGIN = 10;

class GameClient {
private:
    boost::asio::io_context io_context;
    tcp::socket socket;
    PlayerInfo player_info;
    PublicInfo info;
    std::atomic<bool> connected;
    std::vector<char> read_buffer;
    uint32_t expected_message_length;
    bool reading_header;
    std::atomic<bool> has_priority;
    std::vector<CommandCode> available_commands;
    
    // Thread management
    std::thread network_thread;
    std::mutex data_mutex;
    
    // Message queue for UI thread
    std::queue<std::string> message_queue;
    std::mutex queue_mutex;
    
public:
    GameClient() 
        : socket(io_context), connected(false), 
          expected_message_length(0), reading_header(true), 
          has_priority(false) {
        read_buffer.resize(65536); // 64KB buffer
    }
    
    ~GameClient() {
        disconnect();
        if (network_thread.joinable()) {
            network_thread.join();
        }
    }
 
    void connect_to_server(const std::string& host, int port) {
        try {
            auto endpoint = tcp::endpoint(boost::asio::ip::make_address(host), port);
            socket.connect(endpoint);
            connected = true;
            
            // Start network thread
            network_thread = std::thread([this]() {
                network_loop();
            });
        } catch (const std::exception& e) {
            std::cerr << "Connection failed: " << e.what() << "\n";
            connected = false;
        }
    }

    void disconnect() {
        if (connected) {
            connected = false;
            try {
                socket.close();
            } catch (...) {}
            io_context.stop();
        }
    }

    void send_command(const Command& command) {
        if (!connected) {
            push_message("Not connected to server!");
            return;
        } 
        
        // Post the send operation to the network thread
        boost::asio::post(io_context, [this, command]() {
            try {
                // Serialize command to JSON
                nlohmann::json j;
                to_json(j, command);
                std::string json_str = j.dump();
                
                uint32_t len = htonl(static_cast<uint32_t>(json_str.size()));
                
                // Send length first
                boost::asio::write(socket, boost::asio::buffer(&len, sizeof(uint32_t)));
                // Send message
                boost::asio::write(socket, boost::asio::buffer(json_str));
                
                push_message("Command sent: " + command.toString());
            } catch (const std::exception& e) {
                push_message("Send error: " + std::string(e.what()));
                handle_disconnect();
            }
        });
    }

    void push_message(const std::string& message) {
        std::lock_guard<std::mutex> lock(queue_mutex);
        message_queue.push(message);
    }

    bool pop_message(std::string& message) {
        std::lock_guard<std::mutex> lock(queue_mutex);
        if (message_queue.empty()) {
            return false;
        }
        message = message_queue.front();
        message_queue.pop();
        return true;
    }

    bool is_connected() const { return connected; }
    bool has_priority_now() const { return has_priority; }

    std::string open_deck(const std::string& path) {
        try {
            std::ifstream is(path);
            if (!is) {
                return "";
            }
            
            std::string contents((std::istreambuf_iterator<char>(is)),
                               std::istreambuf_iterator<char>());
            is.close();
            return contents;
        } catch (...) {
            return "";
        }
    }

    Command create_command_from_input(CommandCode code, const std::string& param = "") {
        Command cmd;
        cmd.code = code;
        
        switch (code) {      
            case CommandCode::UploadDeck: {
                std::string contents = open_deck(param); 
                if (contents.empty()) {
                    push_message("Failed to load deck from: " + param);
                    cmd.code = CommandCode::Invalid; // Mark as invalid
                } else {
                    cmd.target = contents;
                    push_message("Deck loaded successfully");
                }
                break;
            }   
            case CommandCode::PassPriority:
                break;            
            default:
                break;
        } 
        return cmd;
    }

private:
    void network_loop() {
        try {
            start_read();
            io_context.run();
        } catch (const std::exception& e) {
            push_message("Network thread error: " + std::string(e.what()));
            handle_disconnect();
        }
    }

    void start_read() {
        if (!connected) return;
        
        // Read message length (4 bytes)
        boost::asio::async_read(socket,
            boost::asio::buffer(&expected_message_length, sizeof(uint32_t)),
            [this](boost::system::error_code ec, std::size_t length) {
                if (!ec) {
                    expected_message_length = ntohl(expected_message_length);
                    // Read the actual message
                    boost::asio::async_read(socket,
                        boost::asio::buffer(read_buffer.data(), expected_message_length),
                        [this](boost::system::error_code ec, std::size_t length) {
                            if (!ec) {
                                std::string message(read_buffer.begin(), 
                                                  read_buffer.begin() + expected_message_length);
                                handle_message(message); 
                                start_read(); // Continue reading
                            } else {
                                handle_disconnect();
                            }
                        });
                } else {
                    handle_disconnect();
                }
            });
    }
    
    void handle_message(const std::string& message) {
        // Push message to queue for UI thread to process
        push_message("Server: " + message);
        
        // Handle priority updates
        if (message == "You have priority") {
            has_priority = true;
        } else if (message.find("priority passed") != std::string::npos) {
            has_priority = false;
        }
        
        // Try to parse as JSON for game state updates
        try {
            nlohmann::json j = nlohmann::json::parse(message);
            if (j.contains("priority")) {
                has_priority = (player_info.player_id == j["priority"].get<int>());
            }
        } catch (...) {
            // Not JSON, ignore parsing
        }
    }
    
    void handle_disconnect() {
        if (connected) {
            connected = false;
            push_message("Disconnected from server");
            try {
                socket.close();
            } catch (...) {}
            io_context.stop();
        }
    }
};

// UI Text rendering helper
void render_text(SDL_Renderer* renderer, TTF_Font* font, const std::string& text, 
                 int x, int y, SDL_Color color) {
    SDL_Surface* surface = TTF_RenderText_Blended(font, text.c_str(), color);
    if (surface) {
        SDL_Texture* texture = SDL_CreateTextureFromSurface(renderer, surface);
        SDL_Rect rect = {x, y, surface->w, surface->h};
        SDL_RenderCopy(renderer, texture, nullptr, &rect);
        SDL_FreeSurface(surface);
        SDL_DestroyTexture(texture);
    }
}

// Text input handling
class TextInput {
private:
    std::string text;
    bool active;
    
public:
    TextInput() : active(false) {}
    
    void handle_event(SDL_Event& e) {
        if (e.type == SDL_TEXTINPUT && active) {
            text += e.text.text;
        } else if (e.type == SDL_KEYDOWN && active) {
            if (e.key.keysym.sym == SDLK_BACKSPACE && !text.empty()) {
                text.pop_back();
            }
        }
    }
    
    void set_active(bool is_active) { active = is_active; }
    bool is_active() const { return active; }
    const std::string& get_text() const { return text; }
    void clear() { text.clear(); }
};

// Console message log
class MessageLog {
private:
    std::vector<std::string> messages;
    size_t max_messages;
    
public:
    MessageLog(size_t max = 50) : max_messages(max) {}
    
    void add_message(const std::string& message) {
        messages.push_back(message);
        if (messages.size() > max_messages) {
            messages.erase(messages.begin());
        }
    }
    
    const std::vector<std::string>& get_messages() const { return messages; }
    void clear() { messages.clear(); }
};

int main() {
    try {
        GameClient client;
        
        // Initialize SDL and SDL_ttf
        if (SDL_Init(SDL_INIT_VIDEO) < 0) {
            std::cerr << "SDL could not initialize! SDL_Error: " << SDL_GetError() << "\n";
            return 1;
        }
        
        if (TTF_Init() == -1) {
            std::cerr << "TTF could not initialize! TTF_Error: " << TTF_GetError() << "\n";
            SDL_Quit();
            return 1;
        }
        
        // Create window and renderer
        SDL_Window* window = SDL_CreateWindow("Game Client", 
                                            SDL_WINDOWPOS_UNDEFINED, 
                                            SDL_WINDOWPOS_UNDEFINED, 
                                            WINDOW_WIDTH, WINDOW_HEIGHT, 
                                            SDL_WINDOW_SHOWN);
        if (!window) {
            std::cerr << "Window could not be created! SDL_Error: " << SDL_GetError() << "\n";
            TTF_Quit();
            SDL_Quit();
            return 1;
        }
        
        SDL_Renderer* renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED);
        if (!renderer) {
            std::cerr << "Renderer could not be created! SDL_Error: " << SDL_GetError() << "\n";
            SDL_DestroyWindow(window);
            TTF_Quit();
            SDL_Quit();
            return 1;
        }
        
        // Load font
        TTF_Font* font = TTF_OpenFont("arial.ttf", 16);
        if (!font) {
            // Try fallback font
            font = TTF_OpenFont("/usr/share/fonts/truetype/freefont/FreeMono.ttf", 16);
            if (!font) {
                std::cerr << "Failed to load font! TTF_Error: " << TTF_GetError() << "\n";
                SDL_DestroyRenderer(renderer);
                SDL_DestroyWindow(window);
                TTF_Quit();
                SDL_Quit();
                return 1;
            }
        }
        
        // UI components
        TextInput text_input;
        MessageLog message_log;
        
        // Connect to server
        client.connect_to_server("127.0.0.1", 5000);
        message_log.add_message("Connecting to server...");
        
        // Main game loop
        bool quit = false;
        SDL_Event e;
        
        while (!quit) {
            // Process events
            while (SDL_PollEvent(&e) != 0) {
                if (e.type == SDL_QUIT) {
                    quit = true;
                } else if (e.type == SDL_MOUSEBUTTONDOWN) {
                    // Toggle text input focus
                    SDL_Point mouse_pos = {e.button.x, e.button.y};
                    SDL_Rect input_rect = {MARGIN, WINDOW_HEIGHT - INPUT_HEIGHT - MARGIN, 
                                         WINDOW_WIDTH - 2*MARGIN, INPUT_HEIGHT};
                    text_input.set_active(SDL_PointInRect(&mouse_pos, &input_rect));
                } else if (e.type == SDL_KEYDOWN) {
                    if (e.key.keysym.sym == SDLK_RETURN && text_input.is_active()) {
                        // Send command
                        std::string command_text = text_input.get_text();
                        if (!command_text.empty()) {
                            message_log.add_message("You: " + command_text);
                            
                            // Handle special commands
                            if (command_text == "quit") {
                                client.send_command(Command(CommandCode::Quit));
                            } else if (command_text == "resign") {
                                client.send_command(Command(CommandCode::Resign));
                            } else if (command_text == "pass") {
                                client.send_command(Command(CommandCode::PassPriority));
                            } else if (command_text.find("upload ") == 0) {
                                std::string path = command_text.substr(7);
                                Command cmd = client.create_command_from_input(CommandCode::UploadDeck, path);
                                if (cmd.code != CommandCode::Invalid) {
                                    client.send_command(cmd);
                                }
                            } else {
                                message_log.add_message("Unknown command: " + command_text);
                            }
                            
                            text_input.clear();
                        }
                    }
                }
                
                text_input.handle_event(e);
            }
            
            // Process network messages
            std::string message;
            while (client.pop_message(message)) {
                message_log.add_message(message);
            }
            
            // Clear screen
            SDL_SetRenderDrawColor(renderer, 40, 44, 52, 255);
            SDL_RenderClear(renderer);
            
            // Draw game area (top section)
            SDL_SetRenderDrawColor(renderer, 30, 30, 30, 255);
            SDL_Rect game_rect = {0, 0, WINDOW_WIDTH, WINDOW_HEIGHT - CONSOLE_HEIGHT};
            SDL_RenderFillRect(renderer, &game_rect);
            
            // Draw yellow circle in the middle of game area
            SDL_SetRenderDrawColor(renderer, 255, 255, 0, 255);
            int center_x = WINDOW_WIDTH / 2;
            int center_y = (WINDOW_HEIGHT - CONSOLE_HEIGHT) / 2;
            for (int w = 0; w < 50; w++) {
                for (int h = 0; h < 50; h++) {
                    int dx = w - 25;
                    int dy = h - 25;
                    if (dx*dx + dy*dy <= 25*25) {
                        SDL_RenderDrawPoint(renderer, center_x + dx, center_y + dy);
                    }
                }
            }
            
            // Draw console area
            SDL_SetRenderDrawColor(renderer, 50, 50, 50, 255);
            SDL_Rect console_rect = {0, WINDOW_HEIGHT - CONSOLE_HEIGHT, WINDOW_WIDTH, CONSOLE_HEIGHT};
            SDL_RenderFillRect(renderer, &console_rect);
            
            // Draw message log
            SDL_Color text_color = {255, 255, 255, 255};
            int y_pos = WINDOW_HEIGHT - CONSOLE_HEIGHT + MARGIN;
            const auto& messages = message_log.get_messages();
            for (int i = std::max(0, (int)messages.size() - 10); i < messages.size(); i++) {
                render_text(renderer, font, messages[i], MARGIN, y_pos, text_color);
                y_pos += 20;
            }
            
            // Draw input box
            SDL_SetRenderDrawColor(renderer, text_input.is_active() ? 100 : 70, 70, 70, 255);
            SDL_Rect input_rect = {MARGIN, WINDOW_HEIGHT - INPUT_HEIGHT - MARGIN, 
                                 WINDOW_WIDTH - 2*MARGIN, INPUT_HEIGHT};
            SDL_RenderFillRect(renderer, &input_rect);
            
            // Draw input text
            std::string input_text = "> " + text_input.get_text();
            if (text_input.is_active() && (SDL_GetTicks() / 500) % 2 == 0) {
                input_text += "_";
            }
            render_text(renderer, font, input_text, MARGIN + 5, WINDOW_HEIGHT - INPUT_HEIGHT - MARGIN + 5, text_color);
            
            // Draw status indicators
            if (client.is_connected()) {
                SDL_SetRenderDrawColor(renderer, 0, 255, 0, 255);
            } else {
                SDL_SetRenderDrawColor(renderer, 255, 0, 0, 255);
            }
            SDL_Rect status_rect = {WINDOW_WIDTH - 30, 10, 20, 20};
            SDL_RenderFillRect(renderer, &status_rect);
            
            if (client.has_priority_now()) {
                SDL_SetRenderDrawColor(renderer, 0, 0, 255, 255);
                SDL_Rect priority_rect = {WINDOW_WIDTH - 60, 10, 20, 20};
                SDL_RenderFillRect(renderer, &priority_rect);
                
                // Show priority message
                render_text(renderer, font, "PRIORITY", WINDOW_WIDTH - 150, 12, {0, 0, 255, 255});
            }
            
            // Draw help text
            render_text(renderer, font, "Commands: upload <path>, pass, resign, quit", 
                       MARGIN, 10, {200, 200, 200, 255});
            
            // Present renderer
            SDL_RenderPresent(renderer);
            
            // Cap frame rate
            SDL_Delay(16);
        }
        
        // Cleanup
        client.disconnect();
        TTF_CloseFont(font);
        SDL_DestroyRenderer(renderer);
        SDL_DestroyWindow(window);
        TTF_Quit();
        SDL_Quit();
        
    } catch (std::exception& e) {
        std::cerr << "Client exception: " << e.what() << "\n";
    }
    
    return 0;
}
