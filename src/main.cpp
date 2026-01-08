#include <SDL3/SDL.h>
#include <SDL3/SDL_main.h>
#include <string>

constexpr int screen_width { 960 };
constexpr int screen_height { 540 };

int main() {

    // Initialise an SDL window and two surfaces.
    SDL_Window* window { nullptr };
    SDL_Surface* surface { nullptr };
    SDL_Surface* bitmap_surface { nullptr };

    // Initialise SDL.
    if (SDL_Init(SDL_INIT_VIDEO) == false) {
        SDL_Log("SDL initialisation: %s\n", SDL_GetError());
        return 1;
    }

    // Get window and surface.
    window = SDL_CreateWindow("I am a window :3", screen_width, screen_height, 0);
    if (window == nullptr) {
        SDL_Log("SDL window: %s\n", SDL_GetError());
        return 2;
    } else {
        surface = SDL_GetWindowSurface(window);
    }

    // Load /assets/uwu.bmp.
    std::string image_path { "assets/uwu.bmp" };
    bitmap_surface = SDL_LoadBMP(image_path.c_str());
    if (bitmap_surface == nullptr) {
        SDL_Log("SDL load bitmap: %s\n", SDL_GetError());
        return 3;
    }

    // Simple main loop to keep the window open.
    bool quit {false};
    SDL_Event event;
    SDL_zero(event);
    while (quit == false) {
        // Poll for events.
        while (SDL_PollEvent(&event) == true) {
            if (event.type == SDL_EVENT_QUIT) {
                quit = true;
            }
        }
        
        // Render bitmap :3
        SDL_BlitSurface(bitmap_surface, nullptr, surface, nullptr);
        SDL_UpdateWindowSurface(window);
    }

    // Destroy SDL window and surfaces.
    SDL_DestroySurface(bitmap_surface);
    bitmap_surface = nullptr;
    
    SDL_DestroyWindow(window);
    window = nullptr;
    surface = nullptr;

    // Quit.
    SDL_Quit();

    return 0;
}
