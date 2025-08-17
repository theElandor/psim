#include <iostream>
#include <SDL2/SDL.h>
#include <SDL2/SDL_image.h>
#include <vector>
#include "Scryfall.hpp"
#include <string>
#include <nlohmann/json.hpp>
#include <curl/curl.h>

#define PADDING 20
#define TITLE_PORTION 0.2

using json = nlohmann::json;

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

std::string getCardImageURL(const std::string& jsonString) {
    auto j = json::parse(jsonString);
    if (j.contains("image_uris") && j["image_uris"].contains("normal")) {
        return j["image_uris"]["normal"].get<std::string>();
    }
    return ""; // fallback
}


size_t WriteCallback(void* contents, size_t size, size_t nmemb, void* userp) {
    size_t totalSize = size * nmemb;
    std::vector<unsigned char>* buffer = static_cast<std::vector<unsigned char>*>(userp);
    buffer->insert(buffer->end(), (unsigned char*)contents, (unsigned char*)contents + totalSize);
    return totalSize;
}

std::vector<unsigned char> downloadImage(const std::string& url) {
    CURL* curl = curl_easy_init();
    std::vector<unsigned char> buffer;

    if (curl) {
        curl_easy_setopt(curl, CURLOPT_URL, url.c_str());

        // Make sure to cast to curl_write_callback
        curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, static_cast<size_t(*)(void*,size_t,size_t,void*)>(WriteCallback));
        curl_easy_setopt(curl, CURLOPT_WRITEDATA, &buffer);

        CURLcode res = curl_easy_perform(curl);
        if (res != CURLE_OK) {
            std::cerr << "curl_easy_perform() failed: " << curl_easy_strerror(res) << "\n";
        }

        curl_easy_cleanup(curl);
    }

    return buffer;
}

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
  SDL_SetRenderDrawColor(renderer,
                         c.borderColor.r, c.borderColor.g, 
                         c.borderColor.b, 255);
  SDL_RenderDrawLine(renderer, c.x+PADDING/2, 0, c.x+PADDING/2, win_h);
  SDL_RenderDrawLine(renderer, c.x+col_width-PADDING/2, 0, c.x+col_width-PADDING/2, win_h);
}
void download_random_card(Card &card, SDL_Renderer* renderer){
  ScryfallAPI api;
  std::cout<<"Downloading card information..."<<std::endl;
  std::string card_info = api.getRandomCard();
  std::string url = getCardImageURL(card_info);
  auto imageData = downloadImage(url);
  SDL_Texture *texture = loadTextureFromMemory(renderer,imageData); 
  card.texture = texture;
}
void render_cards(SDL_Renderer* renderer, Column &c, int win_h, int col_width){
  SDL_SetRenderDrawColor(renderer,255,255,255,255); 
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
    // SDL_RenderDrawRect(renderer, &rect);
    SDL_RenderCopy(renderer, c.cards[i].texture, nullptr, &rect);
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
  const int num_cols = 8;
  for(int i = 0; i < num_cols; i++){
    Column col;
    Card sample_card;
    download_random_card(sample_card, renderer);
    sample_card.w = 20; sample_card.h=30;
    col.cards = {sample_card, sample_card, sample_card};
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

