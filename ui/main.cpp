#include <iostream>
#include <SDL2/SDL.h>
#include <SDL2/SDL_image.h>
#include <SDL2/SDL_ttf.h>
#include <vector>
#include "Scryfall.hpp"
#include "Card.hpp"
#include <string>
#include <curl/curl.h>
#include <map>

#define PADDING 20
#define TITLE_PORTION 0.1
#define BUTTON_WIDTH 120
#define BUTTON_HEIGHT 30
#define BUTTON_MARGIN 10
#define PREVIEW_MARGIN 10

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
  /*
    This function parses a deck in the standard MTGO format.
    Probably needs extra refinment and security checks.
    AI kinds of writes very convoluted code, so I wrote this myself.
  */
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

struct Button {
  SDL_Rect rect;
  bool pressed;
  bool hovered;
  bool clicked;
  std::string text;
  SDL_Color color;
  SDL_Color hoverColor;
  SDL_Color clickColor;
  SDL_Color textColor;
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

SDL_Texture* loadTextureFromMemory(SDL_Renderer* renderer, const std::vector<unsigned char>& imageData) {
  /*
   * AI generated. Returns a texture object given a buffer of data.
   */
  SDL_RWops* rw = SDL_RWFromConstMem(imageData.data(), imageData.size());
  if (!rw) {
    std::cerr << "SDL_RWFromConstMem failed: " << SDL_GetError() << "\n";
    return nullptr;
  }

  SDL_Surface* surface = IMG_Load_RW(rw, 1); // 1 = SDL frees the RWops
  if (!surface) {
    std::cerr << "IMG_Load_RW failed: " << IMG_GetError() << "\n";
    return nullptr;
  }

  SDL_Texture* texture = SDL_CreateTextureFromSurface(renderer, surface);
  SDL_FreeSurface(surface);

  return texture;
}

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

void download_random_card(RenderedCard &card, SDL_Renderer* renderer){
  /*
   * Downloads a random card and inserts the information in the
   * card object. Made for debugging purposes as well.
   */
  ScryfallAPI api;
  std::cout<<"Downloading card information..."<<std::endl;
  std::string card_info = api.getRandomCard();
  std::string url = api.getCardImageURL(card_info);
  std::string card_name = api.getCardName(card_info);
  int card_cmc = api.getCardCmc(card_info);
  auto imageData = api.downloadImageCached(url);
  SDL_Texture *texture = loadTextureFromMemory(renderer,imageData); 
  card.texture = texture;
  card.game_info.title = card_name;
  card.game_info.cmc = card_cmc;
}

bool point_in_rect(int x, int y, const SDL_Rect& rect) {
  return x >= rect.x && x < rect.x + rect.w && y >= rect.y && y < rect.y + rect.h;
}

void render_cards(SDL_Renderer* renderer, Column &c, int win_h, int col_width, int mouseX, int mouseY){
  // renders the cards in each column
  SDL_SetRenderDrawColor(renderer,0,0,0,255); 
  float offset;
  for(int i = 0; i < c.cards.size(); i++){
    SDL_Rect rect;
    rect.x = c.x+PADDING/2;
    float dyn_h;
    rect.w = col_width-PADDING; 
    dyn_h = ((float)rect.w/66)*88;
    offset = dyn_h * (TITLE_PORTION);
    rect.h = static_cast<int>(dyn_h);
    rect.y = static_cast<int>(offset*i); 
    // Check if mouse is hovering over this card
    if (point_in_rect(mouseX, mouseY, rect)) {
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

void render_button(SDL_Renderer* renderer, Button &button, TTF_Font* font) {
  // Determine button color based on state
  SDL_Color currentColor = button.color;
  if (button.clicked) {
    currentColor = button.clickColor;
  } else if (button.hovered) {
    currentColor = button.hoverColor;
  }
  // Render button background
  SDL_SetRenderDrawColor(renderer, currentColor.r, currentColor.g, currentColor.b, currentColor.a);
  SDL_RenderFillRect(renderer, &button.rect);
  // Render button border
  SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
  SDL_RenderDrawRect(renderer, &button.rect);
  // Render button text if font is available
  if (font) {
    SDL_Surface* textSurface = TTF_RenderText_Solid(font, button.text.c_str(), button.textColor);
    if (textSurface) {
      SDL_Texture* textTexture = SDL_CreateTextureFromSurface(renderer, textSurface);
      if (textTexture) {
        // Center text in button
        SDL_Rect textRect;
        textRect.w = textSurface->w;
        textRect.h = textSurface->h;
        textRect.x = button.rect.x + (button.rect.w - textRect.w) / 2;
        textRect.y = button.rect.y + (button.rect.h - textRect.h) / 2;
        
        SDL_RenderCopy(renderer, textTexture, nullptr, &textRect);
        SDL_DestroyTexture(textTexture);
      }
      SDL_FreeSurface(textSurface);
    }
  }
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

void group_cards_by_cmc(std::vector<Column>& cols, const std::vector<RenderedCard>& cards, int num_cols) {
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
}

void restore_original_layout(std::vector<Column>& cols, const std::vector<RenderedCard>& cards, int num_cols) {
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
  
  // If deck is empty, create some sample cards for testing
  if (deck.empty()) {
    for (int i = 0; i < 24; i++) { // Create 24 random cards for testing
      Column col;
      col.borderColor = COLORS[i % 4];
      col.x = 0; col.y = 0;
      col.cmc = -1;
      // Add 3 random cards to each column
      for (int j = 0; j < 3; j++) {
        RenderedCard sample_card;
        download_random_card(sample_card, renderer);
        sample_card.w = 20; 
        sample_card.h = 30;
        col.cards.push_back(sample_card);
        allCards.push_back(sample_card);
      }
      cols.push_back(col);
      if (cols.size() >= 8) break; // Limit to 8 columns for testing
    }
    return;
  }
  
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
  // load deck to display
  std::string raw_data = open_deck();
  auto [sample_deck,side] = parse_deck(raw_data); 
  for(auto &c:sample_deck){ 
    std::cout<<c.title<<std::endl;
  }
  int win_w = 1280;
  int win_h = 720;
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
  SDL_Renderer* renderer = SDL_CreateRenderer(window, -1,
                                              SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);
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
  Button cmcButton;
  cmcButton.rect = {win_w - BUTTON_WIDTH - BUTTON_MARGIN, win_h - BUTTON_HEIGHT - BUTTON_MARGIN, BUTTON_WIDTH, BUTTON_HEIGHT};
  cmcButton.text = "Group by CMC";
  cmcButton.color = {200, 200, 200, 255};         // Normal color
  cmcButton.hoverColor = {170, 170, 170, 255};    // Darker when hovered
  cmcButton.clickColor = {255, 255, 0, 255};      // Yellow when clicked
  cmcButton.textColor = {0, 0, 0, 255};
  cmcButton.pressed = false;
  cmcButton.hovered = false;
  cmcButton.clicked = false;
  
  // initialize columns
  initialize_columns(renderer, cols, sample_deck);
  size_t num_cols = cols.size();
  
  while (running) {
    // Get current mouse position for hover detection
    int mouseX, mouseY;
    SDL_GetMouseState(&mouseX, &mouseY);
    cmcButton.hovered = point_in_rect(mouseX, mouseY, cmcButton.rect);
    
    // Reset hovered card each frame
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
        cmcButton.rect.x = win_w - BUTTON_WIDTH - BUTTON_MARGIN;
        cmcButton.rect.y = win_h - BUTTON_HEIGHT - BUTTON_MARGIN;  
      }
      else if (e.type == SDL_MOUSEBUTTONDOWN) {
        if (e.button.button == SDL_BUTTON_LEFT) {
          int clickX = e.button.x;
          int clickY = e.button.y;
          
          if (point_in_rect(clickX, clickY, cmcButton.rect)) {
            cmcButton.clicked = true;
          }
        }
      }
      else if (e.type == SDL_MOUSEBUTTONUP) {
        if (e.button.button == SDL_BUTTON_LEFT) {
          int releaseX = e.button.x;
          int releaseY = e.button.y;
          
          if (cmcButton.clicked && point_in_rect(releaseX, releaseY, cmcButton.rect)) {
            groupedByCMC = !groupedByCMC;
            
            if (groupedByCMC) {
              group_cards_by_cmc(cols, allCards, num_cols);
              cmcButton.text = "Ungroup CMC";
              cmcButton.color = {255, 200, 200, 255}; // Light red when active
              cmcButton.hoverColor = {225, 170, 170, 255}; // Darker hover for active state
            } else {
              restore_original_layout(cols, allCards, num_cols);
              cmcButton.text = "Group by CMC";
              cmcButton.color = {200, 200, 200, 255}; // Gray when inactive
              cmcButton.hoverColor = {170, 170, 170, 255}; // Darker hover for inactive state
            }
          }
          cmcButton.clicked = false;
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
      render_cards(renderer, cols[i], win_h, col_width, mouseX, mouseY);
    }

    // Render the card preview
    render_card_preview(renderer, hoveredCard, win_w, win_h, preview_width, font);
 
    // Render the CMC grouping button
    render_button(renderer, cmcButton, font);
    
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
