#include <iostream>
#include <SDL2/SDL.h>
#include <vector>

#define PADDING 20


struct Card{
  std::string name;
  SDL_Texture* texture;
  int w;
  int h;
};
struct Column{
  std::vector<Card> cards;
  SDL_Color borderColor;
  int x,y;
};
SDL_Color COLORS[4] = {
    {255, 0, 0, 255},   // Red
    {0, 255, 0, 255},   // Green
    {0, 0, 255, 255},   // Blue
    {255, 255, 0, 255}  // Yellow
};

void render_column(SDL_Renderer* renderer, Column &c, int win_h, int col_width){
  SDL_SetRenderDrawColor(renderer,
                         c.borderColor.r, c.borderColor.g, 
                         c.borderColor.b, 255);
  SDL_RenderDrawLine(renderer, c.x+PADDING/2, 0, c.x+PADDING/2, win_h);
  SDL_RenderDrawLine(renderer, c.x+col_width-PADDING/2, 0, c.x+col_width-PADDING/2, win_h);
}

void render_cards(SDL_Renderer* renderer, Column &c, int win_h, int col_width){
  SDL_SetRenderDrawColor(renderer,255,255,255,255); 
  for(int i = 0; i < c.cards.size(); i++){
    SDL_Rect rect;
    rect.x = c.x+PADDING/2;
    rect.y = c.y;
    float dyn_h;
    rect.w = col_width-PADDING; 
    dyn_h = ((float)rect.w/66)*88;
    rect.h = static_cast<int>(dyn_h);
    SDL_RenderDrawRect(renderer, &rect);
  }
}
int main(int argc, char** argv) {
  // Prevent compositor transparency issues
  int win_w = 1280;
  int win_h = 720;
  SDL_SetHint(SDL_HINT_VIDEO_X11_NET_WM_BYPASS_COMPOSITOR, "0");
  if (SDL_Init(SDL_INIT_VIDEO) < 0) {
    std::cout << "Failed to initialize SDL\n";
    return -1;
  }


  SDL_Window* window = SDL_CreateWindow("Deck Visualizer",
                                        SDL_WINDOWPOS_CENTERED,
                                        SDL_WINDOWPOS_CENTERED,
                                        win_w, win_h,
                                        SDL_WINDOW_SHOWN | SDL_WINDOW_RESIZABLE);
  if (!window) {
    std::cout << "Failed to create window\n";
    SDL_Quit();
    return -1;
  }
  SDL_SetWindowOpacity(window, 1.0f);  // force opaque
  SDL_Renderer* renderer = SDL_CreateRenderer(window, -1,
                                              SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);
  if (!renderer) {
    std::cout << "Failed to create renderer\n";
    SDL_DestroyWindow(window);
    SDL_Quit();
    return -1;
  }
  bool running = true;
  SDL_Event e;
  std::vector<Column> cols;
  // initialize columns
  const int num_cols = 10;
  for(int i = 0; i < num_cols; i++){
    Column col;
    Card sample_card;
    sample_card.w = 20; sample_card.h=30;
    col.cards = {sample_card};
    col.borderColor = COLORS[i%4];
    col.x = 0; col.y =0;
    cols.push_back(col); 
  } 
  while (running) {
    while (SDL_PollEvent(&e)) {
      if (e.type == SDL_QUIT) {
          running = false;
      }
      else if (e.type == SDL_WINDOWEVENT &&
               (e.window.event == SDL_WINDOWEVENT_RESIZED ||
                e.window.event == SDL_WINDOWEVENT_SIZE_CHANGED)) {
          win_w = e.window.data1;  // updated width
          win_h = e.window.data2;  // updated height
          std::cout << "Window resized to " << win_w << "x" << win_h << "\n";
      }
    }
    // Clear with black background
    SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
    SDL_RenderClear(renderer);

    // Divide into columns dynamically
    int col_width = win_w / num_cols;

    for (int i = 0; i < num_cols; i++) {
        cols[i].x = i * col_width;
        render_column(renderer, cols[i], win_h, col_width);
        render_cards(renderer, cols[i], win_h, col_width);
    }
    SDL_RenderPresent(renderer);
  }

  SDL_DestroyRenderer(renderer);
  SDL_DestroyWindow(window);
  SDL_Quit();
  return 0;
}

