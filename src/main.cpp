#include <imgui.h>
#include <imgui_impl_sdl3.h>
#include <imgui_impl_sdlrenderer3.h>
#include <SDL3/SDL.h>
#include <SDL3/SDL_main.h>
#include <string>

// Enumeration of possible status values for the application.
enum ApplicationStatus {
    SUCCESS,                // Success.
    INITIALIZATION_ERROR,   // An error occurred during setup, e.g. when creating a window. (SDL errors.)
    RUNTIME_ERROR           // An error occurred during runtime, e.g. when loading a file.
};

// Because RAII is pretty nice <3
class Application {
private:
    ApplicationStatus status;
    int window_width;
    int window_height;
    std::string window_title;
    SDL_Window* window;
    SDL_Renderer* renderer;

public:
    /*
    Print a human-readable message reflecting the current status of the application.

    @return `true` if `status` is `SUCCESS`, `false` otherwise.
    */
    bool print_status() {
        // Get error messages. (e.g. from SDL.)
        std::string sdl_error = SDL_GetError();

        switch (status) {
            case SUCCESS:
                return true;
            case INITIALIZATION_ERROR:
                SDL_Log("Initialization error: %s\n", sdl_error.c_str());
                break;
            case RUNTIME_ERROR:
                if (sdl_error != "") {
                    SDL_Log("SDL runtime error: %s\n", sdl_error.c_str());
                } else {
                    SDL_Log("Runtime error: unknown cause\n");
                }
                break;
            default:
                SDL_Log("Undefined application status: %d\n", status);
        }

        return false;
    }

    /*
    Initialise the application with the provided window dimensions and title.

    @return An `Application` object. Make sure to call `get_status()` on the returned object before 
    using it to find out if initialisation has failed!
    */
    Application(int window_width, int window_height, std::string window_title)
    : window_width(window_width), window_height(window_height), window_title(window_title) {
        // Set exit status to 0 (success).
        status = SUCCESS;

        // Initialize SDL.
        if (SDL_Init(SDL_INIT_VIDEO) == false) {
            status = INITIALIZATION_ERROR;
            return;
        }

        // Create an SDL window.
        float scale;
        if ((scale = scale = SDL_GetDisplayContentScale(SDL_GetPrimaryDisplay())) == 0) {
            scale = 1;  // Use the scaling factor expected by the display based on its DPI settings, default to 1.
        }
        SDL_WindowFlags flags = SDL_WINDOW_HIDDEN | SDL_WINDOW_HIGH_PIXEL_DENSITY | SDL_WINDOW_RESIZABLE;   // The window must be shown explicitly.
        window = SDL_CreateWindow(window_title.c_str(), (int)(window_width * scale), (int)(window_height * scale), flags);
        if (window == nullptr) {
            status = INITIALIZATION_ERROR;
            return;
        }

        // Create an SDL renderer.
        renderer = SDL_CreateRenderer(window, nullptr);
        if (renderer == nullptr) {
            status = INITIALIZATION_ERROR;
            return;
        }
        // Synchronize renderer with each vertical refresh if possible.
        if (SDL_SetRenderVSync(renderer, 1) == false) {
            SDL_Log("SetRenderVSync: %s\n", SDL_GetError());
        }

        // Show window.
        SDL_ShowWindow(window);
    }

    /*
    Run the application.

    @return The `status` value of the `Application` object is set by the function to reflect the state
    of the application when the main loop has been terminated.
    */
    void run() {

        // Load image to render and convert it to a texture.
        SDL_Surface* bmp = SDL_LoadBMP("assets/uwu.bmp");
        if (bmp == nullptr) {
            status = RUNTIME_ERROR;
            return;
        }

        SDL_Texture* texture = SDL_CreateTextureFromSurface(renderer, bmp);
        SDL_DestroySurface(bmp);
        if (texture == nullptr) {
            status = RUNTIME_ERROR;
            return;
        }

        SDL_Event event;
        SDL_zero(event);
        bool quit = false;

        // Enter the main loop.
        while (quit == false) {
            // Event handling.
            while (SDL_PollEvent(&event)) {
                // Close the window when necessary.
                if (event.type == SDL_EVENT_QUIT) {
                    quit = true;
                }
                if (event.type == SDL_EVENT_WINDOW_CLOSE_REQUESTED && event.window.windowID == SDL_GetWindowID(window)) {
                    quit = true;
                }
            }

            // Rendering logic.
            SDL_RenderClear(renderer);
            SDL_RenderTexture(renderer, texture, nullptr, nullptr);
            SDL_RenderPresent(renderer);
        }

        SDL_DestroyTexture(texture);
    }

    ~Application() {
        /*
        SDL cleanup.

        If the application is not properly initialised, e.g. due to an error,
        some fields may be null and that case must be handled properly.

        There might be a cleaner way to do this, but we are not really initialising
        that much stuff so it probably does not matter.
        */
        if (renderer != nullptr) {
            SDL_DestroyRenderer(renderer);
        }
        if (window != nullptr) {
            SDL_DestroyWindow(window);
        }
        SDL_Quit();

        SDL_Log("Finished cleaning up application.");
    }
};

int main() {
    Application application = Application(960, 540, "I am a window :3");

    // Check for initialization errors.
    bool status = application.print_status();
    if (status == true) {
        // Run the application.
        application.run();

        // Checking the return value is unnecessary since main() exits immediately afterwards.
        application.print_status();
    }

    return status;
}
