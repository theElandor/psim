#pragma once
#include "Card.hpp"
#include <SDL2/SDL.h>

struct RenderedCard{
  Card game_info; // Changed from pointer to actual object
  SDL_Texture* texture;
  int w;
  int h;
};
