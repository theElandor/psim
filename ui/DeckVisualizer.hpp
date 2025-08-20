#pragma once
#include <SDL2/SDL.h>
#include <SDL2/SDL_image.h>
#include <SDL2/SDL_ttf.h>
#include "Scryfall.hpp"

#include <vector>
#include "RenderedCard.hpp"
#include "Utils.hpp"

#define PADDING 20
#define TITLE_PORTION 0.1
#define PREVIEW_MARGIN 10
#define BUTTON_WIDTH 120
#define BUTTON_HEIGHT 30
#define BUTTON_MARGIN 10
#define SCROLL_BUFFER 400.0

struct Column{
  std::vector<RenderedCard> cards;
  SDL_Color borderColor;
  int x,y;
  int cmc; // CMC value for this column when grouped
};

class DeckVisualizer{
public: 
  int mouseX = 0;
  int mouseY = 0;
  DeckVisualizer(SDL_Renderer* renderer, TTF_Font* font, SDL_Rect& display_area)
    : renderer(renderer), font(font), area(display_area){
      preview_width = area.w / 4;
  }
  RenderedCard *get_hovered_card(){
    return hoveredCard;
  }
  void renderDeck(std::vector<Card> &deck){
    // Save current viewport/clip state if needed
    if(!columns_initialized){
      initialize_columns(renderer, cols, deck);
    }
    size_t num_cols = cols.size();
    SDL_Rect original_viewport;
    SDL_RenderGetViewport(renderer, &original_viewport);
    SDL_RenderSetViewport(renderer, &area);
    // code here
    SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
    SDL_RenderClear(renderer);

    // Calculate available width for columns (excluding preview area)
    int availableWidth = area.w - preview_width - PREVIEW_MARGIN;
    int col_width = availableWidth / (num_cols > 0 ? num_cols : 1); // Prevent division by zero
    for (int i = 0; i < num_cols; i++) {
      cols[i].x = i * col_width;
      // render_column(renderer, cols[i], win_h, col_width);
      render_cards(renderer, cols[i], area.h, col_width, mouseX, mouseY, scrollOffset);
    } 
    SDL_RenderSetViewport(renderer, &original_viewport);
  }
private:

  void render_column(SDL_Renderer* renderer, Column &c, int win_h, int col_width){
    /*
     * Renders the borders of the columns, which are the card containers.
     * Made for debugging purposes.
     */
    SDL_SetRenderDrawColor(renderer,
                           c.borderColor.r, c.borderColor.g, 
                           c.borderColor.b, 255);
    SDL_RenderDrawLine(renderer, c.x+PADDING/2, 0, c.x+PADDING/2, win_h);
    SDL_RenderDrawLine(renderer, c.x+col_width-PADDING/2, 0, c.x+col_width-PADDING/2, win_h);
  }

    // Calculate the total height needed for all cards in a column
  float calculate_column_content_height(const Column& c, int col_width) {
    if (c.cards.empty()) return 0;   
    float card_height = ((float)(col_width) / 66) * 88;
    float card_offset = card_height * TITLE_PORTION; 
    // Total height = (number of cards - 1) * offset + full card height
    return (c.cards.size() - 1) * card_offset + card_height;
  }

  float get_max_content_height(const std::vector<Column>& cols, int col_width) {
    float maxHeight = 0;
    for (const auto& col : cols) {
      float height = calculate_column_content_height(col, col_width);
      maxHeight = std::max(maxHeight, height);
    }
    return maxHeight;
  }

  void clamp_scroll_offset(float &scrollOffset, int win_w, int win_h, int preview_width, 
                          const std::vector<Column>& cols, size_t num_cols) {    

    int availableWidth = win_w - preview_width - PREVIEW_MARGIN;
    int col_width = availableWidth / (num_cols > 0 ? num_cols : 1);
    float maxContentHeight = get_max_content_height(cols, col_width);
    float viewportHeight = win_h - BUTTON_HEIGHT - BUTTON_MARGIN;
    
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
  clipRect.h = win_h - BUTTON_HEIGHT - BUTTON_MARGIN;
  SDL_RenderSetClipRect(renderer, &clipRect);
  
  for(int i = 0; i < c.cards.size(); i++){
    SDL_Rect rect;
    rect.x = c.x+PADDING/2;
    float dyn_h;
    rect.w = col_width-PADDING; 
    dyn_h = ((float)rect.w/66)*88;
    offset = dyn_h * (TITLE_PORTION);
    rect.h = static_cast<int>(dyn_h);
    rect.y = static_cast<int>(offset*i + scrollOffset);  // Apply scroll offset here
    
    // Only render if the card is visible (within the clipped area)
    if (rect.y + rect.h > 0 && rect.y < win_h - BUTTON_HEIGHT - BUTTON_MARGIN) {
      // Check if mouse is hovering over this card
      if (point_in_rect(mouseX, mouseY, rect) && 
          mouseY < win_h - BUTTON_HEIGHT - BUTTON_MARGIN) { // Don't hover if mouse is over button area
        hoveredCard = &c.cards[i];
        // Draw hover highlight
      }
      SDL_RenderCopy(renderer, c.cards[i].texture, nullptr, &rect);
    }
  }
  
  // Reset clipping
  SDL_RenderSetClipRect(renderer, nullptr);
}

void group_cards_by_cmc(std::vector<Column>& cols, const std::vector<RenderedCard>& cards, int num_cols, float &scrollOffset) {
  // Clear all columns
  for (auto& col : cols) {
    col.cards.clear();
    col.cmc = -1;
  }
  
  // Group cards by CMC
  std::map<int, std::vector<RenderedCard>> cmcGroups;
  for (const auto& card : cards) {
    cmcGroups[card.game_info.cmc].push_back(card);
  }
  
  // Distribute CMC groups to columns
  int colIndex = 0;
  for (auto& pair : cmcGroups) {
    if (colIndex >= num_cols) break;
    
    cols[colIndex].cards = pair.second;
    cols[colIndex].cmc = pair.first;
    colIndex++;
  }
  
  // Reset scroll when grouping changes
  scrollOffset = 0.0f;
}

void restore_original_layout(std::vector<Column>& cols, const std::vector<RenderedCard>& cards, int num_cols, float &scrollOffset) {
    // Clear all columns
  for (auto& col : cols) {
    col.cards.clear();
    col.cmc = -1;
  }
  
  // Distribute cards evenly across columns (original layout)
  int cardsPerCol = cards.size() / num_cols;
  int extraCards = cards.size() % num_cols;
  
  int cardIndex = 0;
  for (int i = 0; i < num_cols && cardIndex < cards.size(); i++) {
    int cardsThisCol = cardsPerCol + (i < extraCards ? 1 : 0);
    for (int j = 0; j < cardsThisCol && cardIndex < cards.size(); j++) {
        cols[i].cards.push_back(cards[cardIndex++]);
    }
  }
  
  // Reset scroll when grouping changes
  scrollOffset = 0.0f;
}

  void insert_set_in_col(SDL_Renderer* renderer, Column &col, std::vector<RenderedCard> &allCards, int size, Card *card){
    RenderedCard sample_card;
    sample_card.game_info = *card; // Copy card data, not pointer assignment
    ScryfallAPI api;
    std::cout<<"Getting card information..."<<std::endl;
    std::string card_info = api.getCardByName(sample_card.game_info.title); // Fixed: use card->title instead of undefined card_name
    std::string url = api.getCardImageURL(card_info);
    sample_card.game_info.cmc = api.getCardCmc(card_info);
    // create texture
    auto imageData = api.downloadImageCached(url);
    SDL_Texture *texture = loadTextureFromMemory(renderer,imageData); 
    sample_card.texture = texture; // Fixed: use sample_card instead of card
    sample_card.w = 20; 
    sample_card.h = 30;
    for(int i = 0; i < size; i++){
      col.cards.push_back(sample_card);
      allCards.push_back(sample_card);
    }
  }

  void initialize_columns(SDL_Renderer* renderer, std::vector<Column> &cols, std::vector<Card> &deck){
    /*
    * This function initially sorts the cards.
    * Each column can contain up to 16 cards.
    * Sets are not split onto multiple columns.
    */
    int next_size = 1; // Fixed: initialize to 1, not 0
    Column col;
    col.x = 0; col.y = 0;
    col.cmc = -1;
    std::string next_set_name = deck[0].title; // Fixed: initialize with first card's title
    
    for(int i = 0; i < deck.size(); i++){
      if(i == 0) {
        // First card, initialize counters
        next_size = 1;
        next_set_name = deck[i].title;
      }
      else if(deck[i].title == next_set_name){ // Same card as previous
        next_size++;
      }
      else{ // New card encountered
        if(col.cards.size() + next_size <= 16){
          // Insert these cards in current column
          insert_set_in_col(renderer, col, allCards, next_size, &deck[i-1]); // Use previous card
          next_size = 1;
          next_set_name = deck[i].title;
        }
        else{ // Start new column
          cols.push_back(col); 
          // Initialize new column
          col.cards.clear();
          col.x = 0; col.y = 0;
          col.cmc = -1;
          insert_set_in_col(renderer, col, allCards, next_size, &deck[i-1]);
          next_size = 1;
          next_set_name = deck[i].title;
        }
      }
      columns_initialized = true;
    }
    
    // Don't forget to add the last batch of cards
    if (next_size > 0 && !deck.empty()) {
      insert_set_in_col(renderer, col, allCards, next_size, &deck.back());
      cols.push_back(col);
    }
  }

  SDL_Renderer* renderer;
  TTF_Font* font;
  int preview_width;

  SDL_Rect &area;

  float scrollOffset = 0.0f;

  std::vector<Column> cols;
  std::vector<RenderedCard> allCards; // Store all cards for regrouping
  RenderedCard* hoveredCard = nullptr; // Pointer to currently hovered card
  bool columns_initialized = false;
};
