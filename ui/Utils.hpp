#pragma once
#include <string>
#include <vector>
#include <iostream>
#include <SDL2/SDL_ttf.h>
#include <SDL2/SDL.h>
#include <SDL2/SDL_image.h>


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

bool point_in_rect(int x, int y, const SDL_Rect& rect) {
  return x >= rect.x && x < rect.x + rect.w && y >= rect.y && y < rect.y + rect.h;
}

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
SDL_Texture* loadTexture(const std::string& path, SDL_Renderer* renderer) {
    SDL_Surface* surface = IMG_Load(path.c_str());
    if (!surface) {
        std::cerr << "Unable to load image " << path << "! SDL_image Error: " << IMG_GetError() << std::endl;
        return nullptr;
    }
    
    SDL_Texture* texture = SDL_CreateTextureFromSurface(renderer, surface);
    SDL_FreeSurface(surface);
    
    if (!texture) {
        std::cerr << "Unable to create texture from " << path << "! SDL Error: " << SDL_GetError() << std::endl;
        return nullptr;
    }
    
    return texture;
}

void renderBackground(SDL_Renderer* renderer, SDL_Texture* backgroundTexture, SDL_Rect area) {
    if (!backgroundTexture) return;
    SDL_RenderCopy(renderer, backgroundTexture, nullptr, &area);
}
void renderIcon(SDL_Renderer* renderer, SDL_Texture* texture, SDL_Rect area) {
    if (!texture) return;

    int texW, texH;
    SDL_QueryTexture(texture, nullptr, nullptr, &texW, &texH);

    float scaleX = (float)area.w / texW;
    float scaleY = (float)area.h / texH;
    float scale = std::min(scaleX, scaleY);  // fit inside

    SDL_Rect icon;
    icon.w = (int)(texW * scale);
    icon.h = (int)(texH * scale);
    icon.x = area.x + (area.w - icon.w) / 2;
    icon.y = area.y + (area.h - icon.h) / 2;

    SDL_RenderCopy(renderer, texture, nullptr, &icon);
}

