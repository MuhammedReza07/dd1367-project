#include <imgui.h>
#include <imgui_impl_sdl3.h>
#include <imgui_impl_sdlrenderer3.h>
#include <SDL3/SDL.h>
#include <SDL3/SDL_main.h>
#include <string>

// Enumeration of possible status values for the application.
enum ApplicationStatus {
    SUCCESS,                // Success.
    INITIALIZATION_ERROR    // An error occurred during setup, e.g. when creating a window. (SDL errors.)
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
    Get the status of the application.

    @return The status of the application as an `ApplicationStatus` value.
    */
    int get_status() {
        return status;
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
        SDL_Window* window = SDL_CreateWindow(window_title.c_str(), (int)(window_width * scale), (int)(window_height * scale), SDL_WINDOW_RESIZABLE);
        if (window == nullptr) {
            status = INITIALIZATION_ERROR;
            return;
        }

        // Create an SDL renderer.
        SDL_Renderer* renderer = SDL_CreateRenderer(window, nullptr);
        if (renderer == nullptr) {
            status = INITIALIZATION_ERROR;
            return;
        }
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

    // Check the application's status before exit.
    int status = application.get_status();
    switch (status) {
        case SUCCESS:
            SDL_Log("Application successfully terminated.");
            break;
        case INITIALIZATION_ERROR:
            SDL_Log("Initialization error: %s\n", SDL_GetError());
            break;
        default:
            SDL_Log("Undefined application status: %d\n", status);
    }

    return status;
}
