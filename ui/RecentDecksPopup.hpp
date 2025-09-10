/*
 * AI generated class. Models a popup screen that allows
 * the user to select a recently uploaded deck.
 * Still need to check the code to look for possible bugs.
 * Will probably need to factor out some stuff to make
 * it a more usable and modular component. Selecting
 * things with a popup might be very usefull.
 * */

#pragma once
#include <SDL2/SDL.h>
#include <SDL2/SDL_ttf.h>
#include <vector>
#include <string>
#include <functional>
#define PADDING_TEXT_BOX 10

class RecentDecksPopup {
private:
  SDL_Renderer* renderer;
  TTF_Font* font;
  std::vector<std::string> recent_decks;
  size_t selected_index;
  bool is_visible;
  
  // Popup dimensions and positioning (will be calculated dynamically)
  int popup_width = 500;
  int popup_height = 400;
  int item_height = 40;
  int padding = 30;
  int border_width = 2;
  
  // Colors
  SDL_Color bg_color = {50, 50, 50, 255};
  SDL_Color border_color = {200, 200, 200, 255};
  SDL_Color text_color = {255, 255, 255, 255};
  SDL_Color selected_color = {80, 120, 180, 255};
  
  // Callbacks
  std::function<void(const std::string&)> on_deck_selected;
  std::function<void()> on_cancelled;
  
  SDL_Rect get_popup_rect(int window_w, int window_h) {
    // Calculate popup size based on content
    int min_width = 400;
    int max_width = std::min(600, window_w - 40);
    
    int title_height = 40;
    int content_height = recent_decks.empty() ? 60 : recent_decks.size() * item_height;
    int button_area_height = 60;
    int total_height = title_height + content_height + button_area_height + (2 * padding);
    
    popup_width = std::max(min_width, std::min(max_width, popup_width));
    popup_height = std::min(window_h - 40, total_height);
    
    return {
        (window_w - popup_width) / 2,
        (window_h - popup_height) / 2,
        popup_width,
        popup_height
    };
  }
  
  SDL_Rect get_item_rect(const SDL_Rect& popup_rect, int index) {
    return {
      popup_rect.x + padding,
      popup_rect.y + padding + 40 + (index * item_height), // 40px for title
      popup_width - (2 * padding),
      item_height - 4 // Small gap between items
    };
  }
  
  std::string get_display_name(const std::string& full_path) {
    // Extract just the filename from the full path
    size_t last_slash = full_path.find_last_of("/\\");
    if (last_slash != std::string::npos && last_slash < full_path.length() - 1) {
      return full_path.substr(last_slash + 1);
    }
    return full_path;
  }
  
  void render_popup_text(const std::string& text, int x, int y, SDL_Color color, bool center = false) {
    SDL_Surface* surface = TTF_RenderText_Solid(font, text.c_str(), color);
    if (!surface) return;
    
    SDL_Texture* texture = SDL_CreateTextureFromSurface(renderer, surface);
    if (texture) {
      SDL_Rect rect;
      rect.w = surface->w;
      rect.h = surface->h;
      rect.x = center ? x - rect.w/2 : x;
      rect.y = y;

      SDL_RenderCopy(renderer, texture, nullptr, &rect);
      SDL_DestroyTexture(texture);
    }
    SDL_FreeSurface(surface);
  }
  
  void render_text_centered(const std::string& text, const SDL_Rect& rect, SDL_Color color) {
    SDL_Surface* surface = TTF_RenderText_Solid(font, text.c_str(), color);
    if (!surface) return;
    
    SDL_Texture* texture = SDL_CreateTextureFromSurface(renderer, surface);
    if (texture) {
      SDL_Rect text_rect;
      text_rect.w = surface->w;
      text_rect.h = surface->h;
      text_rect.x = rect.x + (rect.w - text_rect.w) / 2;
      text_rect.y = rect.y + (rect.h - text_rect.h) / 2;
      
      SDL_RenderCopy(renderer, texture, nullptr, &text_rect);
      SDL_DestroyTexture(texture);
    }
    SDL_FreeSurface(surface);
  }
  
void render_text_left(const std::string& text, const SDL_Rect& rect, SDL_Color color) {
    SDL_Surface* surface = TTF_RenderText_Solid(font, text.c_str(), color);
    if (!surface) return;
    
    SDL_Texture* texture = SDL_CreateTextureFromSurface(renderer, surface);
    if (texture) {
      SDL_Rect text_rect;
      text_rect.w = std::min(surface->w, rect.w - 20); // Leave some margin
      text_rect.h = surface->h;
      text_rect.x = rect.x + 10; // Left margin
      text_rect.y = rect.y + (rect.h - text_rect.h) / 2;
      
      SDL_RenderCopy(renderer, texture, nullptr, &text_rect);
      SDL_DestroyTexture(texture);
    }
    SDL_FreeSurface(surface);
  }

public:
    RecentDecksPopup(SDL_Renderer* renderer, TTF_Font* font) 
        : renderer(renderer), font(font), selected_index(0), is_visible(false) {}
    
    void show(const std::vector<std::string>& decks) {
      recent_decks = decks;
      selected_index = 0;
      is_visible = true;
    }
    
    void hide() {
      is_visible = false;
    }
    
    bool visible() const {
      return is_visible;
    }
    
    void set_on_deck_selected(std::function<void(const std::string&)> callback) {
      on_deck_selected = callback;
    }
   
    void set_on_cancelled(std::function<void()> callback) {
      on_cancelled = callback;
    }
    
    void handle_event(const SDL_Event& event, int window_w, int window_h) {
      if (!is_visible) return;
      
      SDL_Rect popup_rect = get_popup_rect(window_w, window_h);
      
      if (event.type == SDL_KEYDOWN) {
        switch (event.key.keysym.sym) {
          case SDLK_UP:
            if (!recent_decks.empty()) {
                selected_index = (selected_index - 1 + recent_decks.size()) % recent_decks.size();
            }
            break;
          case SDLK_DOWN:
            if (!recent_decks.empty()) {
                selected_index = (selected_index + 1) % recent_decks.size();
            }
            break;
          case SDLK_RETURN:
          case SDLK_SPACE:
            if (on_deck_selected && selected_index < recent_decks.size()) {
                on_deck_selected(recent_decks[selected_index]);
            }
            hide();
            break;
          case SDLK_ESCAPE:
            if (on_cancelled) {
                on_cancelled();
            }
            hide();
            break;
        }
      } else if (event.type == SDL_MOUSEBUTTONDOWN) {
        SDL_Point mouse_pos = {event.button.x, event.button.y};
        
        // Check if click is on a deck item
        bool clicked_on_item = false;
        for (size_t i = 0; i < recent_decks.size(); i++) {
          SDL_Rect item_rect = get_item_rect(popup_rect, i);
          if (SDL_PointInRect(&mouse_pos, &item_rect)) {
            if (on_deck_selected) {
                on_deck_selected(recent_decks[i]);
            }
            hide();
            clicked_on_item = true;
            break;
          }
        }
        
        // If click is outside the popup window but not on an item, close it
        if (!clicked_on_item && !SDL_PointInRect(&mouse_pos, &popup_rect)) {
            if (on_cancelled) {
                on_cancelled();
            }
            hide();
        }
      } else if (event.type == SDL_MOUSEMOTION) {
        SDL_Point mouse_pos = {event.motion.x, event.motion.y};            
        
        // Only update selection if mouse is inside the popup
        if (SDL_PointInRect(&mouse_pos, &popup_rect)) {
        for (size_t i = 0; i < recent_decks.size(); i++) {
            SDL_Rect item_rect = get_item_rect(popup_rect, i);
            if (SDL_PointInRect(&mouse_pos, &item_rect)) {
              selected_index = i;
              break;
            }
          }
        }
      }
    }
    
    void render(int window_w, int window_h) {
      if (!is_visible) return;
      
      // Draw semi-transparent overlay over the ENTIRE screen
      SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);
      SDL_SetRenderDrawColor(renderer, 0, 0, 0, 128);
      SDL_Rect overlay = {0, 0, window_w, window_h};
      SDL_RenderFillRect(renderer, &overlay);
      
      SDL_Rect popup_rect = get_popup_rect(window_w, window_h);
      
      // Draw popup background
      SDL_SetRenderDrawColor(renderer, bg_color.r, bg_color.g, bg_color.b, bg_color.a);
      SDL_RenderFillRect(renderer, &popup_rect);
      
      // Draw popup border
      SDL_SetRenderDrawColor(renderer, border_color.r, border_color.g, border_color.b, border_color.a);
      for (int i = 0; i < border_width; i++) {
        SDL_Rect border_rect = {
          popup_rect.x - i, 
          popup_rect.y - i, 
          popup_rect.w + 2*i, 
          popup_rect.h + 2*i
        };
        SDL_RenderDrawRect(renderer, &border_rect);
      }
      
      // Draw title
      SDL_Rect title_rect = {popup_rect.x, popup_rect.y + 10, popup_rect.w, 30};
      render_text_centered("Recent Decks", title_rect, text_color);
      
      if (recent_decks.empty()) {
        // Show "No recent decks" message
        SDL_Rect message_rect = {
          popup_rect.x + padding,
          popup_rect.y + 60,
          popup_rect.w - 2*padding,
          40
        };
        render_text_centered("No recent decks found", message_rect, text_color);
      } else {
        // Draw deck items
        for (size_t i = 0; i < recent_decks.size(); i++) {
          SDL_Rect item_rect = get_item_rect(popup_rect, i);
          // Draw item background (selected or hover)
          if (i == selected_index) {
            SDL_SetRenderDrawColor(renderer, selected_color.r, selected_color.g, selected_color.b, selected_color.a);
            SDL_RenderFillRect(renderer, &item_rect);
          }
          
          // Draw item text
          std::string display_name = get_display_name(recent_decks[i]);
          render_text_left(display_name, item_rect, text_color);
        }
      }
      
      // Draw instructions at bottom
      SDL_Rect instruction_rect = {
        popup_rect.x,
        popup_rect.y + popup_rect.h - 40,
        popup_rect.w,
        30
      };
      
      if (!recent_decks.empty()) {
        render_popup_text("Arrows Navigate  Enter/Click Select  Esc Cancel", 
                        popup_rect.x + popup_rect.w/2, 
                        instruction_rect.y + 5, 
                        {180, 180, 180, 255}, true);
      } else {
        render_popup_text("Esc Cancel", 
                        popup_rect.x + popup_rect.w/2, 
                        instruction_rect.y + 5, 
                        {180, 180, 180, 255}, true);
      } 
      SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_NONE);
    }
};
