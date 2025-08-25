#pragma once
#include <SDL2/SDL_events.h>
#include <string>
#include "Utils.hpp"
#include <SDL2/SDL.h>
#include <SDL2/SDL_ttf.h>
#include <functional>

class Button {
public:
    Button(SDL_Rect area,
         const std::string& text = "Button",
         SDL_Color color = {200, 200, 200, 255},
         SDL_Color hoverColor = {170, 170, 170, 255},
         SDL_Color clickColor = {140, 140, 140, 255},
         SDL_Color textColor = {0, 0, 0, 255}):
         rect{area},
         pressed(false),
         hovered(false),
         clicked(false),
         text(text),
         color(color),
         hoverColor(hoverColor),
         clickColor(clickColor),
         textColor(textColor),
         onClick(nullptr) {}

    void update_hovered(){
      if(point_in_rect(mouseX, mouseY, rect)){
        hovered = true;
      }
      else{hovered = false;}
    }

    void update_clicked(SDL_Event &e){
      if (point_in_rect(mouseX, mouseY, rect)) {
          if (!clicked && e.type == SDL_MOUSEBUTTONDOWN && e.button.button == SDL_BUTTON_LEFT) {
            setClicked(true);
          }
          else if (clicked && e.type == SDL_MOUSEBUTTONUP && e.button.button == SDL_BUTTON_LEFT) {
              setClicked(false);
              // Only trigger action on mouse up (complete click)
              if (onClick) {
                  onClick();
              }
          }
      } else {
          // Reset clicked state if mouse moves outside button
          if (e.type == SDL_MOUSEBUTTONUP && e.button.button == SDL_BUTTON_LEFT) {
              setClicked(false);
          }
      }
    }

    void render(SDL_Renderer* renderer, TTF_Font* font) {
      // Determine button color based on state
      SDL_Color currentColor = color;
      update_hovered();
      if (clicked) {
        currentColor = clickColor;
      } else if (hovered) {
        currentColor = hoverColor;
      }
      // Render button background
      SDL_SetRenderDrawColor(renderer, currentColor.r, currentColor.g, currentColor.b, currentColor.a);
      SDL_RenderFillRect(renderer, &rect);
      // Render button border
      SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
      SDL_RenderDrawRect(renderer, &rect);
      // Render button text
      if (font) {
        SDL_Surface* textSurface = TTF_RenderText_Solid(font, text.c_str(), textColor);
        if (textSurface) {
          SDL_Texture* textTexture = SDL_CreateTextureFromSurface(renderer, textSurface);
          if (textTexture) {
              SDL_Rect textRect;
              textRect.w = textSurface->w;
              textRect.h = textSurface->h;
              textRect.x = rect.x + (rect.w - textRect.w) / 2;
              textRect.y = rect.y + (rect.h - textRect.h) / 2;

              SDL_RenderCopy(renderer, textTexture, nullptr, &textRect);
              SDL_DestroyTexture(textTexture);
          }
          SDL_FreeSurface(textSurface);
        }
      }
    }
    
    void setPosition(int x, int y){
      rect.x = x;
      rect.y = y;
    }
    
    void setSize(int w, int h){
      rect.w = w;
      rect.h = h;
    }
    void setArea(SDL_Rect area){
      rect = area;
    }
    void setMouse(int x, int y){
      mouseX = x;
      mouseY = y;
    }
    
    // Set callback function for button clicks
    void setOnClick(std::function<void()> callback) {
        onClick = callback;
    }
    
    // Getters for interaction
    SDL_Rect getRect() const { return rect; }
    bool isPressed() const { return pressed; }
    bool isHovered() const { return hovered; }
    bool isClicked() const { return clicked; }
    // State setters
    void setPressed(bool state) { pressed = state; }
    void setHovered(bool state) { hovered = state; }
    void setClicked(bool state) { clicked = state; }
    void setText(const std::string& newText) { text = newText; }
    void setColor(SDL_Color newColor){color = newColor;}
    void setHoverColor(SDL_Color newColor){hoverColor = newColor;}

private:
    int mouseX, mouseY;
    SDL_Rect rect;
    bool pressed;
    bool hovered;
    bool clicked;
    std::string text;
    SDL_Color color;
    SDL_Color hoverColor;
    SDL_Color clickColor;
    SDL_Color textColor;
    std::function<void()> onClick; // Callback function for button clicks
};
