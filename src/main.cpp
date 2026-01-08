#include <imgui.h>
#include <imgui_impl_sdl3.h>
#include <imgui_impl_sdlgpu3.h>
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
        if (surface == nullptr) {
            SDL_Log("SDL get window surface: %s\n", SDL_GetError());
            return 5;
        }
    }

    // Load /assets/uwu.bmp.
    std::string image_path { "assets/uwu.bmp" };
    bitmap_surface = SDL_LoadBMP(image_path.c_str());
    if (bitmap_surface == nullptr) {
        SDL_Log("SDL load bitmap: %s\n", SDL_GetError());
        return 3;
    }

    // Initialise GPU device using SDL to make it cross-platform.
    SDL_GPUDevice* gpu_device = SDL_CreateGPUDevice(
        SDL_GPU_SHADERFORMAT_DXIL | SDL_GPU_SHADERFORMAT_MSL | SDL_GPU_SHADERFORMAT_SPIRV,
        true,   // TODO: Only enable this in debug builds.
        nullptr
    );
    if (gpu_device == nullptr) {
        SDL_Log("SDL initialise GPU: %s\n", SDL_GetError());
        return 4;
    }

    // Claim window for GPU device.
    if (SDL_ClaimWindowForGPUDevice(gpu_device, window) == false) {
        SDL_Log("SDL claim GPU for window: %s\n", SDL_GetError());
        return 6;
    }

    // Set-up ImGUI context.
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();

    // Configure I/O to enable keyboard navigation.
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;

    // Set-up rendering backend.
    ImGui_ImplSDL3_InitForSDLGPU(window);
    ImGui_ImplSDLGPU3_InitInfo sdlgpu3_init_info {};
    sdlgpu3_init_info.Device = gpu_device;
    sdlgpu3_init_info.ColorTargetFormat = SDL_GetGPUSwapchainTextureFormat(gpu_device, window);
    sdlgpu3_init_info.MSAASamples = SDL_GPU_SAMPLECOUNT_1;
    sdlgpu3_init_info.SwapchainComposition = SDL_GPU_SWAPCHAINCOMPOSITION_SDR;
    sdlgpu3_init_info.PresentMode = SDL_GPU_PRESENTMODE_VSYNC;
    ImGui_ImplSDLGPU3_Init(&sdlgpu3_init_info);

    // Simple main loop to keep the window open.
    bool quit {false};
    SDL_Event event;
    SDL_zero(event);
    while (quit == false) {
        // Poll for events.
        while (SDL_PollEvent(&event) == true) {
            // Forward events to the backend.
            ImGui_ImplSDL3_ProcessEvent(&event);
            if (event.type == SDL_EVENT_QUIT) {
                quit = true;
            }
        }
        
        // Render bitmap :3
        // SDL_BlitSurface(bitmap_surface, nullptr, surface, nullptr);
        // SDL_UpdateWindowSurface(window);

        // Start the Dear ImGUI frame.
        ImGui_ImplSDLGPU3_NewFrame();
        ImGui_ImplSDL3_NewFrame();
        ImGui::NewFrame();

        // Show ImGUI demo window.
        ImGui::ShowDemoWindow();

        // Start of rendering the ImGUI frame.
        ImGui::Render();

        // Set-up command buffer and texture for drawing.
        SDL_GPUCommandBuffer* command_buffer = SDL_AcquireGPUCommandBuffer(gpu_device);
        if (command_buffer == nullptr) {
            SDL_Log("SDL get GPU command buffer: %s\n", SDL_GetError());
            continue;
        }
        SDL_GPUTexture* swapchain_texture;
        bool swapchain_texture_acquired;
        swapchain_texture_acquired = SDL_WaitAndAcquireGPUSwapchainTexture(command_buffer, window, &swapchain_texture, nullptr, nullptr);
        if (swapchain_texture_acquired == false || swapchain_texture == nullptr) {
            SDL_Log("SDL get acquire swapchain texture: %s\n", SDL_GetError());
            continue;
        }

        // Prepare draw data.
        ImDrawData* draw_data { ImGui::GetDrawData() };
        ImGui_ImplSDLGPU3_PrepareDrawData(draw_data, command_buffer);

        // Set-up and start a render pass.
        SDL_GPUColorTargetInfo color_target_info = {};
        color_target_info.texture = swapchain_texture;
        color_target_info.clear_color = SDL_FColor { 0.0, 0.0, 0.0, 0.0 };
        color_target_info.load_op = SDL_GPU_LOADOP_CLEAR;
        color_target_info.store_op = SDL_GPU_STOREOP_STORE;
        color_target_info.mip_level = 0;
        color_target_info.layer_or_depth_plane = 0;
        color_target_info.cycle = false;
        SDL_GPURenderPass* render_pass = SDL_BeginGPURenderPass(command_buffer, &color_target_info, 1, nullptr);

        // Render GUI.
        ImGui_ImplSDLGPU3_RenderDrawData(draw_data, command_buffer, render_pass);

        // End of rendering the ImGUI frame.
        SDL_EndGPURenderPass(render_pass);

        // Submit command buffer.
        SDL_SubmitGPUCommandBuffer(command_buffer);
    }

    // Shutdown ImGUI.
    ImGui_ImplSDL3_Shutdown();
    ImGui_ImplSDLGPU3_Shutdown();
    ImGui::DestroyContext();

    // Destroy SDL window and surfaces.
    SDL_WaitForGPUIdle(gpu_device);
    SDL_DestroySurface(bitmap_surface);
    bitmap_surface = nullptr;
    
    SDL_DestroyWindow(window);
    window = nullptr;
    surface = nullptr;

    // Quit.
    SDL_Quit();

    return 0;
}
