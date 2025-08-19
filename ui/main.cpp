#include <iostream>
#include <SDL2/SDL.h>
#include <SDL2/SDL_image.h>
#include <SDL2/SDL_ttf.h>
#include <vector>
#include "Scryfall.hpp"
#include "Card.hpp"
#include "Utils.hpp"
#include "Button.hpp"
#include <string>
#include <curl/curl.h>
#include <map>
#define PADDING 20
#define TITLE_PORTION 0.1
#define PREVIEW_MARGIN 10
#define BUTTON_WIDTH 120
#define BUTTON_HEIGHT 30
#define SCROLL_BUFFER 400.0f

// Temporarly copied function to debug in a easier way.
// ====================================================
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
std::pair<std::vector<Card>, std::vector<Card>> parse_deck(const std::string &raw_data){
  std::vector<Card> main;
  std::vector<Card> side;

 // turn string into vector of cards and assign it to player.
  std::cout<<"parse_deck -> starting deck_parsing..."<<std::endl;
  std::stringstream is(raw_data);
  std::string line;
  int copies;
  bool sideboard = false;
  std::string name;
  while(true){
    if(!std::getline(is,line)){
      std::cout<<"Reached end of list.\n";
      return {main,side};
    }
    std::stringstream is_line(line);
    is_line>>copies;
    is_line.ignore(1);
    if(!std::getline(is_line,name)){
      std::cout<<"Something went wrong during parsing.\n";
      return {{},{}};
    }
    for(int i = 0; i < copies; i++){
      // add card to either sideboard or main deck
      if(sideboard)
        side.emplace_back(0, name, "", "", 0);
      else
        main.emplace_back(0, name, "", "", 0);
    }
    if((int)is.peek() == 13){
      sideboard = true;
      is.ignore(2); // ignore carriage return and newline.
    }
  } 
  return {{},{}};
}
// ====================================================

struct RenderedCard{
  Card game_info; // Changed from pointer to actual object
  SDL_Texture* texture;
  int w;
  int h;
};

struct Column{
  std::vector<RenderedCard> cards;
  SDL_Color borderColor;
  int x,y;
  int cmc; // CMC value for this column when grouped
};

SDL_Color COLORS[4] = {
    {255, 0, 0, 255},   // Red
    {0, 255, 0, 255},   // Green
    {0, 0, 255, 255},   // Blue
    {255, 255, 0, 255}  // Yellow
};

bool groupedByCMC = false;
std::vector<RenderedCard> allCards; // Store all cards for regrouping
RenderedCard* hoveredCard = nullptr; // Pointer to currently hovered card

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
  // tecnically: (number of cards - 1)*(card_height * TITLE_PORTION) + card_heigth
  // might need to review this one.
  if (c.cards.empty()) return 0; 
  float dyn_h = ((float)(col_width) / 66) * 88;
  float offset = dyn_h * TITLE_PORTION; 
  return offset * c.cards.size();
}

// Get the maximum content height across all columns
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
  
  if (maxContentHeight > viewportHeight) {
    float maxScroll = SCROLL_BUFFER;
    float minScroll = viewportHeight - maxContentHeight - SCROLL_BUFFER;
    scrollOffset = std::max(minScroll, std::min(maxScroll, scrollOffset));
  } else {
    // If content fits in viewport, reset scroll to 0
    scrollOffset = 0.0f;
  }
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
        SDL_SetRenderDrawColor(renderer, 255, 255, 255, 100);
        SDL_RenderDrawRect(renderer, &rect);
        SDL_SetRenderDrawColor(renderer, 255, 255, 255, 50);
        SDL_RenderFillRect(renderer, &rect);
      }
      SDL_RenderCopy(renderer, c.cards[i].texture, nullptr, &rect);
    }
  }
  
  // Reset clipping
  SDL_RenderSetClipRect(renderer, nullptr);
}

void render_card_preview(SDL_Renderer* renderer, RenderedCard* card, int win_w, int win_h, int preview_width, TTF_Font* font) {
  if (!card) return;
  
  // Calculate preview area
  SDL_Rect previewArea;
  previewArea.x = win_w - preview_width - PREVIEW_MARGIN;
  previewArea.y = PREVIEW_MARGIN;
  previewArea.w = preview_width;
  previewArea.h = win_h - 2 * PREVIEW_MARGIN - BUTTON_HEIGHT - BUTTON_MARGIN;
  
  // Draw preview background
  SDL_SetRenderDrawColor(renderer, 40, 40, 40, 200);
  SDL_RenderFillRect(renderer, &previewArea);
  SDL_SetRenderDrawColor(renderer, 255, 255, 255, 255);
  SDL_RenderDrawRect(renderer, &previewArea);
  
  // Calculate card dimensions maintaining aspect ratio
  float cardAspect = 66.0f / 88.0f; // Magic card aspect ratio
  int cardWidth = previewArea.w - 2 * PREVIEW_MARGIN;
  int cardHeight = static_cast<int>(cardWidth / cardAspect);
  
  // If card is too tall, scale by height instead
  if (cardHeight > previewArea.h - 100) { // Leave space for text
      cardHeight = previewArea.h - 100;
      cardWidth = static_cast<int>(cardHeight * cardAspect);
  }
  
  SDL_Rect cardRect;
  cardRect.x = previewArea.x + (previewArea.w - cardWidth) / 2;
  cardRect.y = previewArea.y + PREVIEW_MARGIN;
  cardRect.w = cardWidth;
  cardRect.h = cardHeight;
  
  // Render the card
  SDL_RenderCopy(renderer, card->texture, nullptr, &cardRect);
  
  // Render card information text
  if (font) {
    SDL_Color textColor = {255, 255, 255, 255};
    
    // Card name
    SDL_Surface* nameSurface = TTF_RenderText_Solid(font, card->game_info.title.c_str(), textColor);
    if (nameSurface) {
      SDL_Texture* nameTexture = SDL_CreateTextureFromSurface(renderer, nameSurface);
      if (nameTexture) {
          SDL_Rect nameRect;
          nameRect.w = nameSurface->w;
          nameRect.h = nameSurface->h;
          nameRect.x = previewArea.x + (previewArea.w - nameRect.w) / 2;
          nameRect.y = cardRect.y + cardRect.h + 10;
          
          // Ensure text fits within preview area
          if (nameRect.w > previewArea.w - 20) {
              nameRect.w = previewArea.w - 20;
              nameRect.x = previewArea.x + 10;
          }
          
          SDL_RenderCopy(renderer, nameTexture, nullptr, &nameRect);
          SDL_DestroyTexture(nameTexture);
      }
      SDL_FreeSurface(nameSurface);
    } 
      // CMC
    std::string cmcText = "CMC: " + std::to_string(card->game_info.cmc);
    SDL_Surface* cmcSurface = TTF_RenderText_Solid(font, cmcText.c_str(), textColor);
    if (cmcSurface) {
      SDL_Texture* cmcTexture = SDL_CreateTextureFromSurface(renderer, cmcSurface);
      if (cmcTexture) {
          SDL_Rect cmcRect;
          cmcRect.w = cmcSurface->w;
          cmcRect.h = cmcSurface->h;
          cmcRect.x = previewArea.x + (previewArea.w - cmcRect.w) / 2;
          cmcRect.y = cardRect.y + cardRect.h + 35;
          
          SDL_RenderCopy(renderer, cmcTexture, nullptr, &cmcRect);
          SDL_DestroyTexture(cmcTexture);
        }
      SDL_FreeSurface(cmcSurface);
    }
  }
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
  col.borderColor = COLORS[0]; // Fixed: use valid index
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
        col.borderColor = COLORS[cols.size() % 4];
        col.x = 0; col.y = 0;
        col.cmc = -1;
        insert_set_in_col(renderer, col, allCards, next_size, &deck[i-1]);
        next_size = 1;
        next_set_name = deck[i].title;
      }
    }
  }
  
  // Don't forget to add the last batch of cards
  if (next_size > 0 && !deck.empty()) {
    insert_set_in_col(renderer, col, allCards, next_size, &deck.back());
    cols.push_back(col);
  }
}

int main(int argc, char** argv) {
  // deck parsing
  std::string raw_data = open_deck();
  auto [sample_deck,side] = parse_deck(raw_data); 

  // initialize stuff
  int win_w = 1280;
  int win_h = 720;
  float scrollOffset = 0.0f;
  const float SCROLL_SPEED = 30.0f;
  int preview_width = win_w / 4;
  SDL_SetHint(SDL_HINT_VIDEO_X11_NET_WM_BYPASS_COMPOSITOR, "0");
  SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY, "1"); 
  if (SDL_Init(SDL_INIT_VIDEO) < 0) {
    std::cout << "Failed to initialize SDL\n";
    return -1;
  }

  // Initialize TTF for button text
  if (TTF_Init() == -1) {
    std::cout << "Failed to initialize TTF: " << TTF_GetError() << "\n";
    SDL_Quit();
    return -1;
  }

  // Load a font (you might need to adjust the path)
  TTF_Font* font = TTF_OpenFont("/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf", 14);
  if (!font) {
    // Try alternative font paths
    font = TTF_OpenFont("/System/Library/Fonts/Arial.ttf", 14);
    if (!font) {
      std::cout << "Warning: Could not load font: " << TTF_GetError() << "\n";
    }
  }

  SDL_Window* window = SDL_CreateWindow("Deck Visualizer",
                                        SDL_WINDOWPOS_CENTERED,
                                        SDL_WINDOWPOS_CENTERED,
                                        win_w, win_h,
                                        SDL_WINDOW_SHOWN | SDL_WINDOW_RESIZABLE);
  if (!window) {
    std::cout << "Failed to create window\n";
    TTF_Quit();
    SDL_Quit();
    return -1;
  }
  SDL_SetWindowOpacity(window, 1.0f);  // force opaque
  // without VSYNC it's a lot faster!
  SDL_Renderer* renderer = SDL_CreateRenderer(window, -1,
                                              SDL_RENDERER_ACCELERATED);
  if (!renderer) {
    std::cout << "Failed to create renderer\n";
    SDL_DestroyWindow(window);
    TTF_Quit();
    SDL_Quit();
    return -1;
  }
  
  bool running = true;
  SDL_Event e;
  std::vector<Column> cols;
  
  // Create CMC grouping button
  Button cmcButton (win_w - BUTTON_WIDTH - BUTTON_MARGIN, 
        win_h - BUTTON_HEIGHT - BUTTON_MARGIN, 
        BUTTON_WIDTH, BUTTON_HEIGHT,
        "Group by CMC");

  
  // initialize columns
  initialize_columns(renderer, cols, sample_deck);
  size_t num_cols = cols.size();
  
  while (running) {
    // Get current mouse position for hover detection
    int mouseX, mouseY;
    SDL_GetMouseState(&mouseX, &mouseY);
    cmcButton.setHovered(point_in_rect(mouseX, mouseY, cmcButton.getRect())); 
    hoveredCard = nullptr; 
    while (SDL_PollEvent(&e)) {
      if (e.type == SDL_QUIT) {
        running = false;
      }
      else if (e.type == SDL_WINDOWEVENT &&
               (e.window.event == SDL_WINDOWEVENT_RESIZED ||
                e.window.event == SDL_WINDOWEVENT_SIZE_CHANGED)) {
        win_w = e.window.data1;  // updated width
        win_h = e.window.data2;  // updated height
        preview_width = win_w / 4;
        // Update button position
        cmcButton.setPosition(win_w - BUTTON_WIDTH - BUTTON_MARGIN,
                              win_h - BUTTON_HEIGHT - BUTTON_MARGIN);
        // Fix scroll offset after resize
        clamp_scroll_offset(scrollOffset, win_w, win_h, preview_width, cols, num_cols);
      }
      else if (e.type == SDL_MOUSEBUTTONDOWN) {
        if (e.button.button == SDL_BUTTON_LEFT) {
          int clickX = e.button.x;
          int clickY = e.button.y; 
          if (point_in_rect(clickX, clickY, cmcButton.getRect())) {
            cmcButton.setClicked(true);
          }
        }
      }
      else if (e.type == SDL_MOUSEBUTTONUP) {
        if (e.button.button == SDL_BUTTON_LEFT) {
          int releaseX = e.button.x;
          int releaseY = e.button.y;
          
          if (cmcButton.isClicked() && point_in_rect(releaseX, releaseY, cmcButton.getRect())) {
            groupedByCMC = !groupedByCMC;
            
            if (groupedByCMC) {
              group_cards_by_cmc(cols, allCards, num_cols, scrollOffset);
              cmcButton.setText("Ungroup CMC");
              cmcButton.setColor({255, 200, 200, 255});
              cmcButton.setHoverColor({225, 170, 170, 255});
            } else {
              restore_original_layout(cols, allCards, num_cols, scrollOffset);
              cmcButton.setText("Group by CMC");
              cmcButton.setColor({200, 200, 200, 255}); // Gray when inactive
              cmcButton.setHoverColor({170, 170, 170, 255}); // Darker hover for inactive state
            }
          }
          cmcButton.setClicked(false);
        }
      }
      else if (e.type == SDL_MOUSEWHEEL){
          // Handle mouse wheel scrolling
        int availableWidth = win_w - preview_width - PREVIEW_MARGIN;
        int col_width = availableWidth / (num_cols > 0 ? num_cols : 1);
        float maxContentHeight = get_max_content_height(cols, col_width);
        float viewportHeight = win_h - BUTTON_HEIGHT - BUTTON_MARGIN;
        
        // Only allow scrolling if content is taller than viewport
        if (maxContentHeight > viewportHeight) {
          scrollOffset += e.wheel.y * SCROLL_SPEED;
          
          // Clamp scroll offset with buffer space
          float maxScroll = SCROLL_BUFFER;  // Allow scrolling up beyond top
          float minScroll = viewportHeight - maxContentHeight - SCROLL_BUFFER; 
          scrollOffset = std::max(minScroll, std::min(maxScroll, scrollOffset));
        }
      }
    }
    
    // Clear with black background
    SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
    SDL_RenderClear(renderer);

    // Calculate available width for columns (excluding preview area)
    int availableWidth = win_w - preview_width - PREVIEW_MARGIN;
    int col_width = availableWidth / (num_cols > 0 ? num_cols : 1); // Prevent division by zero

    for (int i = 0; i < num_cols; i++) {
      cols[i].x = i * col_width;
      // render_column(renderer, cols[i], win_h, col_width);
      render_cards(renderer, cols[i], win_h, col_width, mouseX, mouseY, scrollOffset);
    }

    // Render the card preview
    render_card_preview(renderer, hoveredCard, win_w, win_h, preview_width, font);
 
    // Render the CMC grouping button
    cmcButton.render(renderer, font); 
    SDL_RenderPresent(renderer);
  }

  if (font) {
    TTF_CloseFont(font);
  }
  TTF_Quit();
  SDL_DestroyRenderer(renderer);
  SDL_DestroyWindow(window);
  SDL_Quit();
  return 0;
}
