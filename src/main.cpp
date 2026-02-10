// Because the application is providing its own entry point.
#define SDL_MAIN_HANDLED 0x39

#include <imgui.h>
#include <imgui_impl_sdl3.h>
#include <imgui_impl_sdlrenderer3.h>
#include <SDL3/SDL.h>
#include <SDL3/SDL_main.h>
#include <string>

// Enumeration of possible status values for the application.
enum ApplicationStatus {
    SUCCESS,                // Success.
    INITIALIZATION_ERROR,   // An error occurred during setup, e.g. when creating a window.
    RUNTIME_ERROR           // An error occurred during runtime, e.g. when loading a file.
};

// File dialog filters (standard template from SDL-wiki, replace with desired formats)
static constexpr SDL_DialogFileFilter filters[] = {
    { "PNG images",  "png" },
    { "JPEG images", "jpg;jpeg" },
    { "All images",  "png;jpg;jpeg" },
    { "TXT files",   "txt" },
    //{ "All files",   "*" }
};

// Callback function used to bring up file explorer dialog
static void SDLCALL callback(void* userdata, const char* const* filelist, const int filter)
{
    if (!filelist) {
        SDL_Log("An error occurred: %s", SDL_GetError());
        return;
    } else if (!*filelist) {
        SDL_Log("The user did not select any file.");
        SDL_Log("Most likely, the dialog was canceled.");
        return;
    }

    while (*filelist) {
        SDL_Log("Full path to selected file: '%s'", *filelist);
        filelist++;
    }

    if (filter < 0) {
        SDL_Log("The current platform does not support fetching "
                "the selected filter, or the user did not select"
                " any filter.");
        return;
    } else if (filter < SDL_arraysize(filters)) {
        SDL_Log("The filter selected by the user is '%s' (%s).",
                filters[filter].pattern, filters[filter].name);
        return;
    }
}

// Because RAII is pretty nice <3
class Application {
private:
    ApplicationStatus status;
    int window_width;
    int window_height;
    float scale;
    std::string window_title;
    SDL_Window* window;
    SDL_Renderer* renderer;

public:
    /*
    Initialize the application with the provided window dimensions and title.

    @return An `Application` object. Make sure to call `get_status()` on the returned object before 
    using it to find out if initialization has failed!
    */
    Application(int window_width, int window_height, std::string window_title)
    : window_width(window_width), window_height(window_height), window_title(window_title) {
        // Set exit status to 0 (success).
        status = SUCCESS;

        // Initialize SDL.
        if (SDL_Init(SDL_INIT_VIDEO) == false) {
            SDL_Log("SDL_Init: %s", SDL_GetError());
            status = INITIALIZATION_ERROR;
            return;
        }

        // Create an SDL window.
        if ((scale = SDL_GetDisplayContentScale(SDL_GetPrimaryDisplay())) == 0) {
            scale = 1;  // Use the scaling factor expected by the display based on its DPI settings, default to 1.
        }
        SDL_WindowFlags flags = SDL_WINDOW_HIDDEN | SDL_WINDOW_HIGH_PIXEL_DENSITY | SDL_WINDOW_RESIZABLE;   // The window must be shown explicitly.
        window = SDL_CreateWindow(window_title.c_str(), (int)(window_width * scale), (int)(window_height * scale), flags);
        if (window == nullptr) {
            SDL_Log("SDL_CreateWindow: %s", SDL_GetError());
            status = INITIALIZATION_ERROR;
            return;
        }

        // Create an SDL renderer.
        renderer = SDL_CreateRenderer(window, nullptr);
        if (renderer == nullptr) {
            SDL_Log("SDL_CreateRenderer: %s", SDL_GetError());
            status = INITIALIZATION_ERROR;
            return;
        }
        // Synchronize renderer with each vertical refresh if possible.
        if (SDL_SetRenderVSync(renderer, 1) == false) {
            SDL_Log("SDL_SetRenderVSync: %s", SDL_GetError());
        }
    }

    /*
    Get the status of the application.

    @return the status of the application as an ApplicationStatus value.
    */
    ApplicationStatus get_status() {
        return status;
    }

    /*
    Run the application.

    @return The `status` value of the `Application` object is set by the function to reflect the state
    of the application when the main loop has been terminated.
    */
    void run() {
        // Show window.
        if (SDL_ShowWindow(window) == false) {
            SDL_Log("SDL_ShowWindow: %s", SDL_GetError());
            status = RUNTIME_ERROR;
            return;
        };

        // Prepare whatever it is that will be rendered to the window.
        
        // Setup ImGui context.
        IMGUI_CHECKVERSION();
        ImGui::CreateContext();
        ImGuiIO& io = ImGui::GetIO();
        io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;

        // Setup ImGui scaling.
        ImGuiStyle& style = ImGui::GetStyle();
        style.ScaleAllSizes(scale);
        style.FontScaleDpi = scale; // Set initial font scale.

        // Setup ImGui platform/renderer backend.
        ImGui_ImplSDL3_InitForSDLRenderer(window, renderer);
        ImGui_ImplSDLRenderer3_Init(renderer);

        // Enter the main loop.
        SDL_Event event;
        SDL_zero(event);
        bool quit = false;

        while (quit == false) {
            // Event handling.
            while (SDL_PollEvent(&event)) {
                // Forward events to the ImGui backend.
                ImGui_ImplSDL3_ProcessEvent(&event);

                // Close the window when necessary.
                if (event.type == SDL_EVENT_QUIT) {
                    quit = true;
                }
                if (event.type == SDL_EVENT_WINDOW_CLOSE_REQUESTED && event.window.windowID == SDL_GetWindowID(window)) {
                    quit = true;
                }
            }

            // Rendering logic.

            // Do no rendering if the window is minimized.
            if (SDL_GetWindowFlags(window) & SDL_WINDOW_MINIMIZED) {
                continue;
            }

            // Start of the ImGui frame.
            ImGui_ImplSDLRenderer3_NewFrame();
            ImGui_ImplSDL3_NewFrame();
            ImGui::NewFrame();

            // Testing-window that brings up file explorer
            ImGui::Begin("Window A");
            ImGui::Text("This is window A");
            ImGui::Text("Click the button below \nto open file explorer");
            if (ImGui::Button("Button A")) {
                printf("Button A clicked!\n");
                SDL_ShowOpenFileDialog(callback,
                    nullptr,
                    window,
                    filters,
                    SDL_arraysize(filters),
                    nullptr,
                    true);
            }
            ImGui::End();

            // Show demo window.
            ImGui::ShowDemoWindow();

            // Render the ImGui frame.
            ImGui::Render();
            SDL_SetRenderScale(renderer, io.DisplayFramebufferScale.x, io.DisplayFramebufferScale.y);
            SDL_SetRenderDrawColorFloat(renderer, 0, 0, 0, 0);
            SDL_RenderClear(renderer);
            ImGui_ImplSDLRenderer3_RenderDrawData(ImGui::GetDrawData(), renderer);
            SDL_RenderPresent(renderer);
        }
    }

    ~Application() {
        // ImGui cleanup.
        ImGui_ImplSDLRenderer3_Shutdown();
        ImGui_ImplSDL3_Shutdown();
        ImGui::DestroyContext();

        /*
        SDL cleanup.

        If the application is not properly initialized, e.g. due to an error,
        some fields may be null and that case must be handled properly.

        There might be a cleaner way to do this, but we are not really initializing
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

    // Check for initialization errors before running.
    if (application.get_status() != SUCCESS) {
        return application.get_status();
    }

    application.run();

    return application.get_status();
}
