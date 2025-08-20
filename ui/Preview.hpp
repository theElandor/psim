#pragma once
#include <SDL2/SDL.h>
#include <SDL2/SDL_ttf.h>
#include <string>
#include "RenderedCard.hpp"

#define PREVIEW_MARGIN 10
class CardPreview {
public:
    CardPreview(SDL_Renderer* renderer, TTF_Font* font, int win_w, int win_h, int preview_width)
        : renderer(renderer), font(font), windowWidth(win_w), windowHeight(win_h), previewWidth(preview_width)
    {
      computePreviewArea();
    }
    CardPreview(){}

    void computePreviewArea(){
      previewArea.x = windowWidth - previewWidth - PREVIEW_MARGIN;
      previewArea.y = PREVIEW_MARGIN;
      previewArea.w = previewWidth;
      // assuming button correction is applied caller side.
      previewArea.h = windowHeight - 2*PREVIEW_MARGIN; 
    }

    void render(RenderedCard* card) {
        if (!card) return;
        computePreviewArea();
        // Draw preview background
        SDL_SetRenderDrawColor(renderer, 40, 40, 40, 200);
        SDL_RenderFillRect(renderer, &previewArea);
        SDL_SetRenderDrawColor(renderer, 255, 255, 255, 255);
        SDL_RenderDrawRect(renderer, &previewArea);

        // Calculate card dimensions maintaining aspect ratio
        float cardAspect = 66.0f / 88.0f; // Magic card aspect ratio
        int cardWidth = previewArea.w - 2 * PREVIEW_MARGIN;
        int cardHeight = static_cast<int>(cardWidth / cardAspect);

        if (cardHeight > previewArea.h - 100) { // Leave space for text
            cardHeight = previewArea.h - 100;
            cardWidth = static_cast<int>(cardHeight * cardAspect);
        }

        SDL_Rect cardRect;
        cardRect.w = cardWidth;
        cardRect.h = cardHeight;
        cardRect.x = previewArea.x + (previewArea.w - cardWidth) / 2;
        cardRect.y = previewArea.y + PREVIEW_MARGIN;

        // Render the card
        SDL_RenderCopy(renderer, card->texture, nullptr, &cardRect);

        if (font) {
            SDL_Color textColor = {255, 255, 255, 255};

            // Render card name
            renderTextCentered(card->game_info.title, textColor, cardRect.y + cardRect.h + 10);

            // Render CMC
            std::string cmcText = "CMC: " + std::to_string(card->game_info.cmc);
            renderTextCentered(cmcText, textColor, cardRect.y + cardRect.h + 35);
        }
    }
    void setWindowSize(int win_w, int win_h, int preview_w){
      windowWidth = win_w;
      windowHeight = win_h;
      previewWidth = preview_w;
      computePreviewArea();
    }
private:
    SDL_Renderer* renderer;
    TTF_Font* font;
    int windowWidth;
    int windowHeight;
    int previewWidth;
    SDL_Rect previewArea;

    // Helper function to render text centered in previewArea
    void renderTextCentered(const std::string& text, SDL_Color color, int y) {
        SDL_Surface* surface = TTF_RenderText_Solid(font, text.c_str(), color);
        if (!surface) return;

        SDL_Texture* texture = SDL_CreateTextureFromSurface(renderer, surface);
        if (texture) {
            SDL_Rect rect;
            rect.w = surface->w;
            rect.h = surface->h;
            rect.x = previewArea.x + (previewArea.w - rect.w) / 2;
            rect.y = y;

            // Ensure text fits
            if (rect.w > previewArea.w - 20) {
                rect.w = previewArea.w - 20;
                rect.x = previewArea.x + 10;
            }

            SDL_RenderCopy(renderer, texture, nullptr, &rect);
            SDL_DestroyTexture(texture);
        }
        SDL_FreeSurface(surface);
    }
};

