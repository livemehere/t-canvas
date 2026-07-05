#include <iostream>

#include "SDL3/SDL.h"
#include "SDL3/SDL_main.h"
#include "SDL3_ttf/SDL_ttf.h"

void DrawText(SDL_Renderer *renderer, TTF_Font *font, const std::string &text, const SDL_Point pos, SDL_Color color) {
    SDL_Surface *surface = TTF_RenderText_Blended(
        font,
        text.c_str(),
        0,
        color);
    SDL_Texture *texture = SDL_CreateTextureFromSurface(renderer, surface);
    SDL_FRect dst{
        (float) pos.x,
        (float) pos.y,
        (float) surface->w,
        (float) surface->h
    };
    SDL_RenderTexture(renderer, texture, nullptr, &dst);
}

int main() {
    if (!SDL_Init(SDL_INIT_VIDEO | SDL_INIT_EVENTS) || !TTF_Init()) {
        return -1;
    }

    SDL_Window *window = SDL_CreateWindow(
        "TCanvas", 1280, 720, 0);

    if (!window) {
        SDL_Log("Creating Window failed: %s", SDL_GetError());
        SDL_Quit();
        return 1;
    }

    SDL_Renderer *renderer = SDL_CreateRenderer(window, nullptr);
    if (!renderer) {
        SDL_Log("Creating Renderer failed: %s", SDL_GetError());
        SDL_DestroyWindow(window);
        SDL_Quit();
        return 1;
    }


    TTF_Font *font = TTF_OpenFont("/System/Library/Fonts/Supplemental/Arial.ttf", 16);
    if (!font) {
        SDL_Log("Font loading failed: %s", SDL_GetError());
        return 1;
    }

    bool running = true;
    SDL_Event event;

    while (running) {
        while (SDL_PollEvent(&event)) {
            if (event.type == SDL_EVENT_QUIT) {
                running = false;
            }
        }

        SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
        SDL_RenderClear(renderer);

        // begin --

        // Rect
        SDL_FRect rect{
            10.0f, 10.0f, 100.0f, 100.0f
        };
        SDL_SetRenderDrawColor(renderer, 80, 180, 255, 255);
        SDL_RenderFillRect(renderer, &rect);

        // Text
        DrawText(renderer, font, "Hello world", {10, 10}, {255, 255, 255, 255});

        // end --

        SDL_RenderPresent(renderer);
    }


    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    SDL_Quit();
    return 0;
}


