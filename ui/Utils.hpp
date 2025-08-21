#pragma once
#include <string>
#include <vector>
#include <iostream>
#include <SDL2/SDL_ttf.h>
#include <SDL2/SDL.h>

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
