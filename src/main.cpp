#include <iostream>

#include <SDL3/SDL.h>
#include <SDL3/SDL_main.h>
#include <SDL3_ttf/SDL_ttf.h>
#include <SDL3_image/SDL_image.h>

void UpdateDpr(SDL_Window *window, SDL_Renderer *renderer, float &dpr) {
    // DPR --
    int ww, wh;
    int pw, ph;
    int rw, rh;
    float sx, sy;
    SDL_GetWindowSize(window, &ww, &wh);
    SDL_GetWindowSizeInPixels(window, &pw, &ph);
    SDL_GetCurrentRenderOutputSize(renderer, &rw, &rh);
    SDL_GetRenderScale(renderer, &sx, &sy);
    dpr = static_cast<float>(pw) / static_cast<float>(ww);
    SDL_Log("window=%dx%d pixels=%dx%d render=%dx%d dpr=%f scale=%f,%f",
            ww, wh, pw, ph, rw, rh,
            (float) pw / (float) ww,
            sx, sy);
    // DPR --
    SDL_SetRenderScale(renderer, dpr, dpr);
}

void DrawImage(SDL_Renderer *renderer, const float dpr, const std::string &filePath, const SDL_Point &pos,
               const SDL_Color &color) {
    // TODO: cache created texture
    SDL_Surface *surface = IMG_Load(filePath.c_str());
    if (!surface) {
        SDL_Log("Image loading failed: %s", SDL_GetError());
        return;
    }

    SDL_Texture *texture = SDL_CreateTextureFromSurface(renderer, surface);
    const SDL_FRect dst{
        static_cast<float>(pos.x),
        static_cast<float>(pos.y),
        static_cast<float>(surface->w),
        static_cast<float>(surface->h)
    };

    SDL_SetTextureColorMod(texture, color.r, color.g, color.b);
    SDL_SetTextureAlphaMod(texture, color.a);

    SDL_RenderTexture(renderer, texture, nullptr, &dst);
    SDL_DestroySurface(surface);

    SDL_SetTextureColorMod(texture, 255, 255, 255);
    SDL_SetTextureAlphaMod(texture, 255);
}

void DrawText(SDL_Renderer *renderer, TTF_Font *font, const float dpr, const std::string &text, const SDL_Point &pos,
              const SDL_Color &color) {
    // TODO: cache created texture
    SDL_Surface *surface = TTF_RenderText_Blended(
        font,
        text.c_str(),
        0,
        {255, 255, 255, 255});
    SDL_Texture *texture = SDL_CreateTextureFromSurface(renderer, surface);

    const SDL_FRect dst{
        static_cast<float>(pos.x),
        static_cast<float>(pos.y),
        static_cast<float>(surface->w) / dpr,
        static_cast<float>(surface->h) / dpr
    };
    SDL_SetTextureColorMod(texture, color.r, color.g, color.b);
    SDL_SetTextureAlphaMod(texture, color.a);

    SDL_RenderTexture(renderer, texture, nullptr, &dst);
    SDL_DestroySurface(surface);

    SDL_SetTextureColorMod(texture, 255, 255, 255);
    SDL_SetTextureAlphaMod(texture, 255);
}

void DrawRect(SDL_Renderer *renderer, const float dpr, const SDL_FRect &rect, const SDL_Color &color) {
    const SDL_FRect _rect{
        rect.x, rect.y, rect.w, rect.h
    };
    SDL_SetRenderDrawColor(renderer, color.r, color.g, color.b, color.a);
    SDL_RenderFillRect(renderer, &_rect);
}

int main() {
    if (!SDL_Init(SDL_INIT_VIDEO | SDL_INIT_EVENTS)) {
        return -1;
    }

    if (!TTF_Init()) {
        return -1;
    }

    SDL_Window *window = SDL_CreateWindow(
        "TCanvas", 1280, 720, SDL_WINDOW_HIGH_PIXEL_DENSITY);

    if (!window) {
        SDL_Log("Creating Window failed: %s", SDL_GetError());
        SDL_Quit();
        return 1;
    }
    SDL_SetWindowPosition(window, 2560 + 100, 100);

    SDL_Renderer *renderer = SDL_CreateRenderer(window, nullptr);
    if (!renderer) {
        SDL_Log("Creating Renderer failed: %s", SDL_GetError());
        SDL_DestroyWindow(window);
        SDL_Quit();
        return 1;
    }

    float dpr = 1;
    UpdateDpr(window, renderer, dpr);

    TTF_Font *font = TTF_OpenFont("/System/Library/Fonts/Supplemental/Arial.ttf", 16 * dpr);
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
            if (event.type == SDL_EVENT_WINDOW_MOVED) {
                UpdateDpr(window, renderer, dpr);
            }
        }

        SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
        SDL_RenderClear(renderer);

        // begin --

        // Rect
        DrawRect(renderer, dpr, {10.0f, 10.0f, 100.0f, 100.0f}, {80, 20, 255, 255});
        DrawRect(renderer, dpr, {200.0f, 10.0f, 100.0f, 100.0f}, {80, 20, 255, 255});

        // Text
        DrawText(renderer, font, dpr, "Hello world", {10, 10}, {80, 255, 255, 255});
        DrawText(renderer, font, dpr, "Canvas", {200, 10}, {255, 255, 255, 255});
        DrawImage(renderer, dpr, "../../assets/img.png", {700, 10}, {80, 255, 255, 255});

        // end --

        SDL_RenderPresent(renderer);
    }


    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    SDL_Quit();
    return 0;
}


