#pragma once
#include <SDL2/SDL.h>
#include <SDL2/SDL_ttf.h>
#include <string>
#include "RenderedCard.hpp"

#define PREVIEW_MARGIN 10

class Preview {
public:
    Preview(SDL_Renderer* renderer, TTF_Font* font, SDL_Rect& preview_area)
        : renderer(renderer), font(font), area(preview_area) {}
    
    // Update preview area when window is resized
    void update_area(SDL_Rect& new_area) {
        area = new_area;
    }
    
    // Render the preview for a given card
    void render(RenderedCard* card) {
        if (!card || !font) return; 
        // Set viewport to preview area
        SDL_Rect original_viewport;
        SDL_RenderGetViewport(renderer, &original_viewport);
        SDL_RenderSetViewport(renderer, &area);
        
        // Draw preview background
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
        
        // Render text
        SDL_Color textColor = {255, 255, 255, 255};
        render_text(card->game_info.title, textColor, cardRect.y + cardRect.h + 10);
        
        std::string cmcText = "CMC: " + std::to_string(card->game_info.cmc);
        render_text(cmcText, textColor, cardRect.y + cardRect.h + 35);
        
        SDL_RenderSetViewport(renderer, &original_viewport);
    }

private:
    void render_text(const std::string& text, SDL_Color color, int y) {
        SDL_Surface* surface = TTF_RenderText_Solid(font, text.c_str(), color);
        if (!surface) return;
        
        SDL_Texture* texture = SDL_CreateTextureFromSurface(renderer, surface);
        if (texture) {
            SDL_Rect rect;
            rect.w = surface->w;
            rect.h = surface->h;
            rect.x = (area.w - rect.w) / 2;  // Viewport-relative centering
            rect.y = y;

            // Ensure text fits
            if (rect.w > area.w - 20) {
                rect.w = area.w - 20;
                rect.x = 10;  // Viewport-relative left margin
            }

            SDL_RenderCopy(renderer, texture, nullptr, &rect);
            SDL_DestroyTexture(texture);
        }
        SDL_FreeSurface(surface);
    }

    SDL_Renderer* renderer;
    TTF_Font* font;
    SDL_Rect& area;
};
