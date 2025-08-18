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
#define TITLE_PORTION 0.2
#define BUTTON_WIDTH 120
#define BUTTON_HEIGHT 30
#define BUTTON_MARGIN 10
#define PREVIEW_MARGIN 10

struct RenderedCard{
  Card card;
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
  // renders the borders of columns.
  SDL_SetRenderDrawColor(renderer,
                         c.borderColor.r, c.borderColor.g, 
                         c.borderColor.b, 255);
  SDL_RenderDrawLine(renderer, c.x+PADDING/2, 0, c.x+PADDING/2, win_h);
  SDL_RenderDrawLine(renderer, c.x+col_width-PADDING/2, 0, c.x+col_width-PADDING/2, win_h);
}

void download_random_card(RenderedCard &card, SDL_Renderer* renderer){
  // uses the scryfall api to download a random card and set 
  // the texture. Used for debug purposes.
  ScryfallAPI api;
  std::cout<<"Downloading card information..."<<std::endl;
  std::string card_info = api.getRandomCard();
  std::string url = api.getCardImageURL(card_info);
  std::string card_name = api.getCardName(card_info);
  int card_cmc = api.getCardCmc(card_info);
  auto imageData = api.downloadImage(url);
  SDL_Texture *texture = loadTextureFromMemory(renderer,imageData); 
  card.texture = texture;
  card.card.title = card_name;
  card.card.cmc = card_cmc;
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
    SDL_Surface* nameSurface = TTF_RenderText_Solid(font, card->card.title.c_str(), textColor);
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
    std::string cmcText = "CMC: " + std::to_string(card->card.cmc);
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
    cmcGroups[card.card.cmc].push_back(card);
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

int main(int argc, char** argv) {
  // Prevent compositor transparency issues
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
  const int num_cols = 6;
  for(int i = 0; i < num_cols; i++){
    Column col;
    RenderedCard sample_card;
    download_random_card(sample_card, renderer);
    sample_card.w = 20; sample_card.h=30;
    col.cards = {sample_card, sample_card, sample_card};
    col.borderColor = COLORS[i%4];
    col.x = 0; col.y =0;
    col.cmc = -1;
    cols.push_back(col);
    
    // Store all cards for regrouping
    for (const auto& card : col.cards) {
        allCards.push_back(card);
    }
  } 
  
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
        
        std::cout << "Window resized to " << win_w << "x" << win_h << "\n";
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
    int col_width = availableWidth / num_cols;

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
