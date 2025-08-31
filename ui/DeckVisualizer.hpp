#pragma once
#include <SDL2/SDL.h>
#include <SDL2/SDL_image.h>
#include <SDL2/SDL_ttf.h>
#include "Scryfall.hpp"
#include "Preview.hpp"

#include <vector>
#include <thread>
#include <atomic>
#include <mutex>
#include <queue>
#include "RenderedCard.hpp"
#include "Utils.hpp"

#define PADDING 20
#define TITLE_PORTION 0.1
#define TOP_MARGIN 10
#define SIDE_MARGIN 10
#define SCROLL_BUFFER 400.0

struct Column{
  std::vector<RenderedCard> cards;
  SDL_Color borderColor;
  int x,y;
  int cmc; // CMC value for this column when grouped
};

struct CardLoadTask {
  Card card_info;
  int copies;
  size_t column_index;
  size_t task_id;
};
struct LoadedCard {
    std::string title;
    int cmc;
    std::vector<unsigned char> image_data; // Raw image data
    int copies;
    size_t column_index;
    size_t task_id;
};
enum class LoadingState {
  IDLE,
  LOADING,
  COMPLETED,
  ERROR
};

class DeckVisualizer{
public: 
  int mouseX = 0;
  int mouseY = 0;
  
  DeckVisualizer(SDL_Renderer* renderer, TTF_Font* font, SDL_Rect& display_area)
    : renderer(renderer), font(font), area(display_area), 
      loading_state(LoadingState::IDLE), total_tasks(0), completed_tasks(0) {
      preview_width = area.w / 4;
      update_areas();
      preview = new Preview(renderer, font, preview_area);
  }
  
  ~DeckVisualizer() {
    // Stop background loading
    stop_loading();
    delete preview;
  }

  void reset_for_new_deck() {
    stop_loading();
    columns_initialized = false;
    cols.clear();
    allCards.clear();
    hoveredCard = nullptr;
    scrollOffset = 0.0f;
    loading_state = LoadingState::IDLE;
  }

  // Update areas when window is resized
  void update_display_area(SDL_Rect& new_area) {
    area = new_area;
    preview_width = area.w / 4;
    update_areas();
    if (preview) {
      preview->update_area(preview_area);
    }
  }
  
  RenderedCard *get_hovered_card(){
    return hoveredCard;
  }
  
  // Handle mouse wheel scrolling
  void handle_scroll(int scroll_y) {
    float scroll_speed = 30.0f;
    scrollOffset += scroll_y * scroll_speed;
    
    // Clamp scroll offset
    clamp_scroll_offset();
  }
  void setMouse(int x, int y){
    mouseX = x;
    mouseY = y;
  } 
  void renderDeck(std::vector<Card> &deck){
    if(!columns_initialized && loading_state == LoadingState::IDLE){
      initialize_columns_async(deck);
    }
    
    // Process any completed card loads
    process_completed_loads();
    
    // Save current viewport/clip state
    SDL_Rect original_viewport;
    SDL_RenderGetViewport(renderer, &original_viewport);
    SDL_RenderSetViewport(renderer, &area);
    
    // Show loading popup if still loading
    if (loading_state == LoadingState::LOADING) {
      render_loading_popup();
    } else if (loading_state == LoadingState::COMPLETED || loading_state == LoadingState::ERROR) {
      // Render deck columns
      render_deck_columns();
      
      // Render preview
      if (preview && hoveredCard) {
        preview->render(hoveredCard);
      }
    }
    
    SDL_RenderSetViewport(renderer, &original_viewport);
  }

private:
  void update_areas() {
    // Calculate deck area (left side, excluding preview)
    deck_area.x = 0;
    deck_area.y = TOP_MARGIN;
    deck_area.w = area.w - preview_width - SIDE_MARGIN;
    deck_area.h = area.h - TOP_MARGIN;
    
    // Calculate preview area (right side)
    preview_area.x = deck_area.w + SIDE_MARGIN;
    preview_area.y = TOP_MARGIN;
    preview_area.w = preview_width - PREVIEW_MARGIN;
    preview_area.h = deck_area.h - TOP_MARGIN; 
  }

  void render_loading_popup() {
    // Calculate popup dimensions
    int popup_w = 300;
    int popup_h = 150;
    int popup_x = (area.w - popup_w) / 2;
    int popup_y = (area.h - popup_h) / 2;
    
    // Draw semi-transparent overlay
    SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);
    SDL_SetRenderDrawColor(renderer, 0, 0, 0, 128);
    SDL_Rect overlay = {0, 0, area.w, area.h};
    SDL_RenderFillRect(renderer, &overlay);
    
    // Draw popup background
    SDL_SetRenderDrawColor(renderer, 60, 60, 60, 255);
    SDL_Rect popup_bg = {popup_x, popup_y, popup_w, popup_h};
    SDL_RenderFillRect(renderer, &popup_bg);
    
    // Draw popup border
    SDL_SetRenderDrawColor(renderer, 200, 200, 200, 255);
    SDL_RenderDrawRect(renderer, &popup_bg);
    
    // Draw loading text
    if (font) {
      SDL_Color text_color = {255, 255, 255, 255};
      std::string loading_text = "Loading cards...";
      render_popup_text(loading_text, popup_x + popup_w/2, popup_y + 40, text_color, true);
      
      // Draw progress
      if (total_tasks > 0) {
        float progress = (float)completed_tasks / total_tasks;
        std::string progress_text = std::to_string(completed_tasks) + "/" + std::to_string(total_tasks);
        render_popup_text(progress_text, popup_x + popup_w/2, popup_y + 70, text_color, true);
        
        // Draw progress bar
        int bar_w = popup_w - 40;
        int bar_h = 20;
        int bar_x = popup_x + 20;
        int bar_y = popup_y + 90;
        
        // Progress bar background
        SDL_SetRenderDrawColor(renderer, 40, 40, 40, 255);
        SDL_Rect bar_bg = {bar_x, bar_y, bar_w, bar_h};
        SDL_RenderFillRect(renderer, &bar_bg);
        
        // Progress bar fill
        SDL_SetRenderDrawColor(renderer, 0, 150, 0, 255);
        SDL_Rect bar_fill = {bar_x, bar_y, (int)(bar_w * progress), bar_h};
        SDL_RenderFillRect(renderer, &bar_fill);
        
        // Progress bar border
        SDL_SetRenderDrawColor(renderer, 200, 200, 200, 255);
        SDL_RenderDrawRect(renderer, &bar_bg);
      }
    }
    
    SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_NONE);
  }
  
  void render_popup_text(const std::string& text, int center_x, int y, SDL_Color color, bool center = false) {
    SDL_Surface* surface = TTF_RenderText_Solid(font, text.c_str(), color);
    if (!surface) return;
    
    SDL_Texture* texture = SDL_CreateTextureFromSurface(renderer, surface);
    if (texture) {
      SDL_Rect rect;
      rect.w = surface->w;
      rect.h = surface->h;
      rect.x = center ? center_x - rect.w/2 : center_x;
      rect.y = y;

      SDL_RenderCopy(renderer, texture, nullptr, &rect);
      SDL_DestroyTexture(texture);
    }
    SDL_FreeSurface(surface);
  }

  void render_deck_columns() {
    size_t num_cols = cols.size();
    if (num_cols == 0) return;
    
    // Set viewport to deck area only
    SDL_Rect original_viewport;
    SDL_RenderGetViewport(renderer, &original_viewport);
    SDL_RenderSetViewport(renderer, &deck_area);
    
    int col_width = deck_area.w / num_cols;
    
    // Reset hovered card at start of render
    hoveredCard = nullptr;
    
    for (size_t i = 0; i < num_cols; i++) {
      cols[i].x = i * col_width;
      render_cards(renderer, cols[i], deck_area.h, col_width, 
                   mouseX - deck_area.x, mouseY - deck_area.y, scrollOffset);
    }
    
    SDL_RenderSetViewport(renderer, &original_viewport);
  }

  // Calculate the total height needed for all cards in a column
  float calculate_column_content_height(const Column& c, int col_width) {
    if (c.cards.empty()) return 0;   
    float card_height = ((float)(col_width) / 66) * 88;
    float card_offset = card_height * TITLE_PORTION; 
    // Total height = (number of cards - 1) * offset + full card height
    return (c.cards.size() - 1) * card_offset + card_height;
  }

  float get_max_content_height(int col_width) {
    float maxHeight = 0;
    for (const auto& col : cols) {
      float height = calculate_column_content_height(col, col_width);
      maxHeight = std::max(maxHeight, height);
    }
    return maxHeight;
  }

  void clamp_scroll_offset() {    
    if (cols.empty()) {
      scrollOffset = 0.0f;
      return;
    }
    
    int col_width = deck_area.w / cols.size();
    float maxContentHeight = get_max_content_height(col_width);
    float viewportHeight = deck_area.h;
    
    if (maxContentHeight <= viewportHeight) {
        // Content fits entirely in viewport - no scrolling needed
        scrollOffset = 0.0f;
        return;
    } 
    
    // Calculate scroll limits
    float maxScrollUp = 0.0f;  // Can't scroll above the top of first card
    float maxScrollDown = viewportHeight - maxContentHeight;  // Can scroll until last card is visible
    
    // Clamp the scroll offset
    scrollOffset = std::max(maxScrollDown, std::min(maxScrollUp, scrollOffset));
  }

  void render_cards(SDL_Renderer* renderer, Column &c, int win_h, int col_width, int mouseX, int mouseY, float scrollOffset){
    // renders the cards in each column with scroll support
    SDL_SetRenderDrawColor(renderer,0,0,0,255); 
    float offset;
    
    // Create clipping rectangle for this column
    SDL_Rect clipRect;
    clipRect.x = c.x;
    clipRect.y = 0;
    clipRect.w = col_width;
    clipRect.h = win_h;
    SDL_RenderSetClipRect(renderer, &clipRect);
    
    for(size_t i = 0; i < c.cards.size(); i++){
      SDL_Rect rect;
      rect.x = c.x+PADDING/2;
      float dyn_h;
      rect.w = col_width-PADDING; 
      dyn_h = ((float)rect.w/66)*88;
      offset = dyn_h * (TITLE_PORTION);
      rect.h = static_cast<int>(dyn_h);
      rect.y = static_cast<int>(offset*i + scrollOffset);  // Apply scroll offset here
      
      // Only render if the card is visible (within the clipped area)
      if (rect.y + rect.h > 0 && rect.y < win_h) {
        SDL_RenderCopy(renderer, c.cards[i].texture, nullptr, &rect);
        
        // Check if mouse is hovering over this card (after rendering)
        if (point_in_rect(mouseX, mouseY, rect) && 
            mouseY < win_h) { // Don't hover if mouse is over button area
          hoveredCard = &c.cards[i];
        }
      }
    }
    
    // Reset clipping
    SDL_RenderSetClipRect(renderer, nullptr);
  }

  void initialize_columns_async(std::vector<Card>& deck) {
    loading_state = LoadingState::LOADING;
    total_tasks = 0;
    completed_tasks = 0;
    task_counter = 0;
    
    // Clear previous data
    cols.clear();
    allCards.clear();
    
    // Group cards by name and count
    std::map<std::string, int> cardCounts;
    for (const auto& card : deck) {
      cardCounts[card.title]++;
    }
    
    // Create columns and tasks
    Column col;
    col.x = 0; col.y = 0; col.cmc = -1;
    size_t current_column = 0;
    
    for (const auto& pair : cardCounts) {
      if (col.cards.size() + pair.second > 16) {
        // Start new column
        if (!col.cards.empty()) {
          cols.push_back(col);
          current_column++;
        }
        col.cards.clear();
        col.x = 0; col.y = 0; col.cmc = -1;
      }
      
      // Create task for this card
      CardLoadTask task;
      task.card_info.title = pair.first;
      task.copies = pair.second;
      task.column_index = current_column;
      task.task_id = task_counter++;
      
      // Add placeholder cards to column
      for (int i = 0; i < pair.second; i++) {
        RenderedCard placeholder;
        placeholder.game_info.title = pair.first;
        placeholder.game_info.cmc = -1; // Mark as not loaded
        placeholder.texture = nullptr;
        placeholder.w = 0;
        placeholder.h = 0;
        col.cards.push_back(placeholder);
      }
      {
        std::lock_guard<std::mutex> lock(task_mutex);
        pending_tasks.push(task);
        total_tasks++;
      }
    }
    
    // Add the last column
    if (!col.cards.empty()) {
      cols.push_back(col);
    }
    
    // Start background thread
    stop_loading();  // Stop any existing thread
    loading_thread = std::thread(&DeckVisualizer::load_cards_background, this);
  }
  
void load_cards_background() {
  ScryfallAPI api; 
  while (true) {
    CardLoadTask task;
    bool has_task = false;
    
    {
      std::lock_guard<std::mutex> lock(task_mutex);
      if (!pending_tasks.empty()) {
          task = pending_tasks.front();
          pending_tasks.pop();
          has_task = true;
      }
    }
    
    if (!has_task) {
      loading_state = LoadingState::COMPLETED;
      columns_initialized = true;
      break;
    }
    
    try {
      // Load card data
      std::string card_info = api.getCardByName(task.card_info.title);
      std::string url = api.getCardImageURL(card_info);
      int cmc = api.getCardCmc(card_info);
      
      // Download image data (not texture)
      auto imageData = api.downloadImageCached(url);
      
      // Create loaded card with image data
      LoadedCard loaded_card;
      loaded_card.title = task.card_info.title;
      loaded_card.cmc = cmc;
      loaded_card.image_data = imageData; // Store raw data
      loaded_card.copies = task.copies;
      loaded_card.column_index = task.column_index;
      loaded_card.task_id = task.task_id;
      
      {
          std::lock_guard<std::mutex> lock(completed_mutex);
          completed_loads.push(loaded_card);
      }
      
      completed_tasks++;
        
    } catch (const std::exception& e) {
        std::cerr << "Error loading card " << task.card_info.title << ": " << e.what() << std::endl;
        completed_tasks++;
    }
  }
}
  
void process_completed_loads() {
  std::lock_guard<std::mutex> lock(completed_mutex);
  
  while (!completed_loads.empty()) {
    LoadedCard loaded = completed_loads.front();
    completed_loads.pop();
    
    // Create texture in main thread
    SDL_Texture* texture = loadTextureFromMemory(renderer, loaded.image_data);
    
    if (texture && loaded.column_index < cols.size()) {
        // Update cards with matching name
      int updated = 0;
      for (auto& card : cols[loaded.column_index].cards) {
        if (card.game_info.title == loaded.title && updated < loaded.copies) {
          card.game_info.cmc = loaded.cmc;
          card.texture = texture;
          card.w = 20;  // Set appropriate dimensions
          card.h = 30;
          allCards.push_back(card);
          updated++;
        }
      }
    }
  }
}

  void stop_loading() {
    if (loading_thread.joinable()) {
      // Clear remaining tasks to signal thread to stop
      {
        std::lock_guard<std::mutex> lock(task_mutex);
        while (!pending_tasks.empty()) {
          pending_tasks.pop();
        }
      }
      loading_thread.join();
    }
  }

  SDL_Renderer* renderer;
  TTF_Font* font;
  int preview_width;
  Preview* preview;

  SDL_Rect &area;          // Total area
  SDL_Rect deck_area;      // Area for deck columns
  SDL_Rect preview_area;   // Area for preview

  float scrollOffset = 0.0f;

  std::vector<Column> cols;
  std::vector<RenderedCard> allCards; // Store all cards for regrouping
  RenderedCard* hoveredCard = nullptr; // Pointer to currently hovered card
  bool columns_initialized = false;
  
  // Threading for card loading
  std::atomic<LoadingState> loading_state;
  std::thread loading_thread;
  std::queue<CardLoadTask> pending_tasks;
  std::queue<LoadedCard> completed_loads;
  std::mutex task_mutex;
  std::mutex completed_mutex;
  std::atomic<size_t> total_tasks;
  std::atomic<size_t> completed_tasks;
  size_t task_counter;
};
