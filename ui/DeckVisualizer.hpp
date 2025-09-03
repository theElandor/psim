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

// Card size scaling constants
#define MIN_CARD_SCALE 0.5f
#define MAX_CARD_SCALE 3.0f
#define CARD_SCALE_STEP 0.1f

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
      loading_state(LoadingState::IDLE), total_tasks(0), completed_tasks(0), card_scale(2.0f) {
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
    horizontalScrollOffset = 0.0f;  // Reset horizontal scroll too
    loading_state = LoadingState::IDLE;
    // Keep card_scale - don't reset it so user's preference persists
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
  
  // Handle mouse wheel scrolling with CTRL+scroll for card scaling
  void handle_scroll(int scroll_y, bool ctrl_pressed = false, bool shift_pressed = false) {
    if (ctrl_pressed) {
      // Scale card size
      if (scroll_y > 0) {
        card_scale += CARD_SCALE_STEP;
      } else {
        card_scale -= CARD_SCALE_STEP;
      }
      // Clamp card scale
      card_scale = std::max(MIN_CARD_SCALE, std::min(MAX_CARD_SCALE, card_scale));
      // Clamp both scroll offsets since content size changed
      clamp_scroll_offsets();
    } else if (shift_pressed) {
      // Horizontal scrolling
      float scroll_speed = 30.0f;
      horizontalScrollOffset += scroll_y * scroll_speed;
      clamp_scroll_offsets();
    } else {
      // Regular vertical scrolling
      float scroll_speed = 30.0f;
      scrollOffset += scroll_y * scroll_speed;
      clamp_scroll_offsets();
    }
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
      render_deck_columns();
      // Render card scale indicator if not at default size
      if (card_scale != 1.0f) {
        render_scale_indicator();
      } 
      // Render preview
      if (preview) {
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

  void render_scale_indicator() {
    // Show card scale in corner
    SDL_Color text_color = {255, 255, 255, 200};
    std::string scale_text = "Card Size: " + std::to_string((int)(card_scale * 100)) + "%";
    
    // Render in top-right corner of deck area
    int text_x = deck_area.w - 120;
    int text_y = 10;
    
    // Semi-transparent background
    SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);
    SDL_SetRenderDrawColor(renderer, 0, 0, 0, 128);
    SDL_Rect bg_rect = {text_x - 5, text_y - 2, 110, 20};
    SDL_RenderFillRect(renderer, &bg_rect);
    
    render_popup_text(scale_text, text_x, text_y, text_color, false);
    SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_NONE);
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
    
    // Calculate column width based on card scale (minimum spacing between columns)
    int base_card_width = 100; // Base card width
    int scaled_card_width = static_cast<int>(base_card_width * card_scale);
    int col_spacing = 20; // Minimum spacing between columns
    int col_width = scaled_card_width + col_spacing;
    
    // Reset hovered card at start of render
    hoveredCard = nullptr;
    
    for (size_t i = 0; i < num_cols; i++) {
      int col_x = static_cast<int>(i * col_width + horizontalScrollOffset);
      cols[i].x = col_x;
      
      // Only render columns that are visible
      if (col_x + col_width > 0 && col_x < deck_area.w) {
        render_cards(renderer, cols[i], deck_area.h, col_width, 
                     mouseX - deck_area.x, mouseY - deck_area.y, scrollOffset);
      }
    }
    
    SDL_RenderSetViewport(renderer, &original_viewport);
  }

  // Calculate the total height needed for all cards in a column (with card scaling)
  float calculate_column_content_height(const Column& c, int col_width) {
    if (c.cards.empty()) return 0;   
    float card_height = ((float)(col_width) / 66) * 88 * card_scale;
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

  float get_total_content_width() {
    if (cols.empty()) return 0;
    
    int base_card_width = 100;
    int scaled_card_width = static_cast<int>(base_card_width * card_scale);
    int col_spacing = 20;
    int col_width = scaled_card_width + col_spacing;
    
    return cols.size() * col_width;
  }

  void clamp_scroll_offsets() {    
    if (cols.empty()) {
      scrollOffset = 0.0f;
      horizontalScrollOffset = 0.0f;
      return;
    }
    
    // Clamp vertical scroll offset
    int base_card_width = 100;
    int scaled_card_width = static_cast<int>(base_card_width * card_scale);
    int col_spacing = 20;
    int col_width = scaled_card_width + col_spacing;
    
    float maxContentHeight = get_max_content_height(col_width);
    float viewportHeight = deck_area.h;
    
    if (maxContentHeight <= viewportHeight) {
        scrollOffset = 0.0f;
    } else {
        float maxScrollUp = 0.0f;
        float maxScrollDown = viewportHeight - maxContentHeight;
        scrollOffset = std::max(maxScrollDown, std::min(maxScrollUp, scrollOffset));
    }
    
    // Clamp horizontal scroll offset
    float totalContentWidth = get_total_content_width();
    float viewportWidth = deck_area.w;
    
    if (totalContentWidth <= viewportWidth) {
        horizontalScrollOffset = 0.0f;
    } else {
        float maxScrollLeft = 0.0f;
        float maxScrollRight = viewportWidth - totalContentWidth;
        horizontalScrollOffset = std::max(maxScrollRight, std::min(maxScrollLeft, horizontalScrollOffset));
    }
  }

  void render_cards(SDL_Renderer* renderer, Column &c, int win_h, int col_width, int mouseX, int mouseY, float scrollOffset){
    // renders the cards in each column with scroll support and card scaling
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
      
      // Apply card scaling to both width and height
      int base_card_width = 100;  // Base card width
      int scaled_width = static_cast<int>(base_card_width * card_scale);
      float scaled_height = ((float)scaled_width/66)*88;  // Maintain card aspect ratio
      
      // Center the scaled card horizontally in the column
      rect.x = c.x + (col_width - scaled_width) / 2;
      rect.w = scaled_width;
      rect.h = static_cast<int>(scaled_height);
      
      offset = scaled_height * (TITLE_PORTION);
      rect.y = static_cast<int>(offset*i + scrollOffset);  // Apply scroll offset here
      
      // Only render if the card is visible (within the clipped area)
      if (rect.y + rect.h > 0 && rect.y < win_h && rect.x + rect.w > 0 && rect.x < deck_area.w) {
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
    /*
     * Function called only once (at the first render call). It 
     * creates tasks that will be executed by the loading thread.
     * This is done because in SDL the main thread always needs to 
     * render UI, otherwise it will be detected as "non responding"
     * by most OSs.
     * Tasks are placed in a shared queue thanks to a mutex that makes
     * the insertion safe (since the queue is used by both this function and
     * the background thread).
     * ML
    */
    size_t cards_per_col = 16;
    if(deck.size() <= 15){
      // then we are rendering sideboard
      cards_per_col = 4;
    }
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
      if (col.cards.size() + pair.second > cards_per_col) {
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
  /*
   * This function is used in a background thread called as the 
   * client first calls the render() function. Pops tasks 
   * from a shared queue which is filled in the main thread
   * with all the cards that need to be loaded. It leverages
   * the Scryfall module to either download cards using the API
   * or just load them from disk if they are available in the data folder.
   * ML
  */
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
  /*
   * This function retrieves cards that have been 
   * successfully loaded in the background thread.
   * It creates the texture and fills the card information
   * so that it can be rendered in the main thread.
   * ML
  */
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
  float horizontalScrollOffset = 0.0f;  // New horizontal scroll offset

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

  float card_scale;        // Card size scaling factor
};
