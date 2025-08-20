#pragma once
#include <SDL2/SDL.h>
#include <SDL2/SDL_rect.h>
#include <SDL2/SDL_ttf.h>
#include <string>
#include "RenderedCard.hpp"
#define PREVIEW_MARGIN 10

class CardPreview{
public:
  CardPreview(SDL_Renderer *renderer, TTF_Font* font, SDL_Rect &area):
  renderer(renderer), font(font), area(area){
  }
  // preview margin is applied caller side.

  void render(RenderedCard* card) {
    if (!card) return;
    
    // Save and set viewport
    SDL_Rect original_viewport;
    SDL_RenderGetViewport(renderer, &original_viewport);
    SDL_RenderSetViewport(renderer, &area);    

    // Draw preview background (use local rect since viewport is set)
    SDL_Rect bg_rect = {0, 0, area.w, area.h};
    SDL_SetRenderDrawColor(renderer, 40, 40, 40, 200);
    SDL_RenderFillRect(renderer, &bg_rect);
    SDL_SetRenderDrawColor(renderer, 255, 255, 255, 255);
    SDL_RenderDrawRect(renderer, &bg_rect);

    // Calculate card dimensions maintaining aspect ratio
    float cardAspect = 66.0f / 88.0f; // Magic card aspect ratio
    int cardWidth = area.w - 2 * PREVIEW_MARGIN;
    int cardHeight = static_cast<int>(cardWidth / cardAspect);

    if (cardHeight > area.h - 100) { // Leave space for text
        cardHeight = area.h - 100;
        cardWidth = static_cast<int>(cardHeight * cardAspect);
    }

    // Position card (viewport-relative coordinates)
    SDL_Rect cardRect;
    cardRect.w = cardWidth;
    cardRect.h = cardHeight;
    cardRect.x = (area.w - cardWidth) / 2;  // Centered horizontally
    cardRect.y = PREVIEW_MARGIN;            // Top margin

    // Render the card
    SDL_RenderCopy(renderer, card->texture, nullptr, &cardRect);
    
    if (font) {
        SDL_Color textColor = {255, 255, 255, 255};
        // Render card name (pass viewport-relative Y coordinate)
        renderTextCentered(card->game_info.title, textColor, cardRect.y + cardRect.h + 10);
        // Render CMC
        std::string cmcText = "CMC: " + std::to_string(card->game_info.cmc);
        renderTextCentered(cmcText, textColor, cardRect.y + cardRect.h + 35);
    }
    
    // Restore original viewport
    SDL_RenderSetViewport(renderer, &original_viewport);
  } 

  void renderTextCentered(const std::string& text, SDL_Color color, int y) {
      SDL_Surface* surface = TTF_RenderText_Solid(font, text.c_str(), color);
      if (!surface) return;
      SDL_Texture* texture = SDL_CreateTextureFromSurface(renderer, surface);
      if (texture) {
          SDL_Rect rect;
          rect.w = surface->w;
          rect.h = surface->h;
          rect.x = area.x + (area.w - rect.w) / 2;
          rect.y = y;

          // Ensure text fits
          if (rect.w > area.w - 20) {
              rect.w = area.w - 20;
              rect.x = area.x + 10;
          }

          SDL_RenderCopy(renderer, texture, nullptr, &rect);
          SDL_DestroyTexture(texture);
      }
      SDL_FreeSurface(surface);
  }
private:
  SDL_Renderer *renderer;
  TTF_Font *font;
  SDL_Rect &area;

};
