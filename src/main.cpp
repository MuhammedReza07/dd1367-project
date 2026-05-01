// Because the application is providing its own entry point.
#define SDL_MAIN_HANDLED 0x39  // NOLINT

#include <SDL3/SDL.h>
#include <SDL3/SDL_dialog.h>
#include <SDL3/SDL_main.h>
#include <SDL3_image/SDL_image.h>
#include <backends/imgui_impl_sdl3.h>
#include <backends/imgui_impl_sdlrenderer3.h>
#include <imgui.h>
#include <imgui_internal.h>
#include <imgui_node_editor.h>
#include <libbc7enc.h>

#include <array>
#include <string>
#include <vector>

namespace NodeImGui = ax::NodeEditor;

// Enumeration of possible status values for the application.
enum ApplicationStatus {
	SUCCESS,			   // Success.
	INITIALIZATION_ERROR,  // An error occurred during setup, e.g. when creating
						   // a window.
	RUNTIME_ERROR  // An error occurred during runtime, e.g. when loading a
				   // file.
};

// File dialog filters.
constexpr std::array<SDL_DialogFileFilter, 4> dialog_filters = {
	SDL_DialogFileFilter{"PNG images", "png"},
	SDL_DialogFileFilter{"JPEG images", "jpg;jpeg"},
	SDL_DialogFileFilter{"All images", "png;jpg;jpeg"},
	SDL_DialogFileFilter{
		"TXT files",
		"txt"},	 // TXT-files are nice for debugging, let them stay for now
};

std::vector<std::string> selected_files;  // Vector (aka C++ ArrayList) to store
										  // the files selected by the user
// Store unprocessed textures
std::vector<SDL_Texture*> original_textures;
// Store post-processed textures
std::vector<SDL_Texture*> manipulated_textures;

// Callback function used to bring up file explorer dialog
static void SDLCALL callback(void* userdata, const char* const* filelist,
							 const int filter) {
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
		std::string filepath =
			*filelist;	// Temporary variable to avoid memory-issues
		selected_files.push_back(
			filepath);	// Add the selected filepath to the vector
		original_textures.push_back(IMG_LoadTexture(
			static_cast<SDL_Renderer*>(userdata), filepath.c_str()));
		filelist++;	 // Keep iterating through the selected files
	}

	if (filter < 0) {
		SDL_Log(
			"The current platform does not support fetching "
			"the selected filter, or the user did not select"
			" any filter.");
		return;
	} else if (filter < SDL_arraysize(dialog_filters)) {
		SDL_Log("The filter selected by the user is '%s' (%s).",
				dialog_filters.data()[filter].pattern,
				dialog_filters.data()[filter].name);
		return;
	}
}

// Function to process the selected images
static void processImages(SDL_Renderer* renderer) {
	// Loop through all selected files
	for (const auto& file : selected_files) {
		SDL_Surface* surface = IMG_Load(file.c_str());	// Create surface
		surface = SDL_ConvertSurface(surface, SDL_PIXELFORMAT_RGBA32);
		if (surface != nullptr) {
			encode_output output;
			rdo_bc::rdo_bc_params params;
			params.m_dxgi_format = DXGI_FORMAT_BC7_UNORM;
			bc7enc_compress_image_from_memory(surface->w, surface->h,
											  surface->pixels, params, &output);
			printf("compressed! blocks %d bpb %d mips %d\n", output.num_blocks,
				   output.bytes_per_block, output.mipmap_count);

			bc7enc_write_encode_output_to_dds("test.dds", &output, true, false);
			bc7enc_free_encode_output(&output);

			// ADD PROCESSING LOGIC HERE!
			// SDL_Texture* newTexture =  // Create texture of manipulated
			// surface 	SDL_CreateTextureFromSurface(renderer, surface);
			SDL_Texture* newTexture = IMG_LoadTexture(renderer, "test.dds");
			if (newTexture != nullptr) {
				manipulated_textures.push_back(newTexture);
			}
			SDL_DestroySurface(surface);  // Free up memory
		}
	}
}

// Because RAII is pretty nice <3
class Application {
    private:
	ApplicationStatus status;
	float scale;
	std::string window_title;
	SDL_Window* window;
	SDL_Renderer* renderer;

	class NodeEditor {
		NodeImGui::EditorContext* nodeContext;
		int uniqueId = 1;

	   public:
		int uniquePinId = 1;
		NodeImGui::NodeId spawnNodeId = 0;
		struct Link {
			NodeImGui::LinkId id;
			NodeImGui::PinId startPin;
			NodeImGui::PinId endPin;
		};
		struct Node {
			NodeImGui::NodeId id;
			ImVector<NodeImGui::PinId> inputs;
			ImVector<NodeImGui::PinId> outputs;
			// lägg till typ field för nod-typer?
		};

		NodeImGui::NodeId getUniqueId() {
			return NodeImGui::NodeId(uniqueId++);
		}

		NodeImGui::PinId getUniquePinId() {
			return NodeImGui::PinId(uniquePinId++);
		}

		NodeEditor() {
			NodeImGui::Config config;
			config.SettingsFile = "NodeEditor.json";
			nodeContext = NodeImGui::CreateEditor(&config);
		}

		ImVector<Link> links;  // list for saving links between nodes
		ImVector<Node> nodes;  // list for saving nodes

		void addNode(Node node) { nodes.push_back(node); }

		void addLink(Link link) { links.push_back(link); }

		void removeNode(NodeImGui::NodeId nodeId) {
			for (int i = 0; i < nodes.size(); ++i) {
				if (nodes[i].id == nodeId) {
					nodes.erase(nodes.begin() + i);
					break;
				}
			}
		}

		void removeLink(NodeImGui::LinkId linkId) {
			for (int i = 0; i < links.size(); ++i) {
				if (links[i].id == linkId) {
					links.erase(links.begin() + i);
					break;
				}
			}
		}

		void cleanup() { NodeImGui::DestroyEditor(nodeContext); }
	
		void render() {
			NodeImGui::SetCurrentEditor(nodeContext);
			NodeImGui::Begin("Node Editor");

			for (const auto& node : nodes) {
				NodeImGui::BeginNode(node.id);

				ImGui::Text("Node %d", node.id.Get());
				
				// IF YOU TRY TO ADD THE PINS, THE PROGRAM FREEZES
				for (const auto& inputPin : node.inputs) {
					//NodeImGui::BeginPin(inputPin, NodeImGui::PinKind::Input);
					ImGui::Text("%d",
								inputPin.Get());  // PRINT THE PIN ID TO CHECK
												  // IF IT IS WORKING (IT DOES NOT)
					//NodeImGui::EndPin();
				}

				for (const auto& outputPin : node.outputs) {
					//NodeImGui::BeginPin(outputPin, NodeImGui::PinKind::Output);
					ImGui::Text("%d", outputPin.Get());
					//NodeImGui::EndPin();
				}

				NodeImGui::EndNode();
			}


			NodeImGui::End();
		}

		void createLink(NodeImGui::PinId startPin, NodeImGui::PinId endPin) {
			Link newLink;
			newLink.id = NodeImGui::LinkId(uniqueId++);
			newLink.startPin = startPin;
			newLink.endPin = endPin;
			addLink(newLink);
		}
		
	};

	NodeEditor nodeEditor;


    public:
	/**
	Initialize the application with the provided window dimensions and title.

	@return An `Application` object. Make sure to call `get_status()` on the
	returned object before using it to find out if initialization has failed!
	*/
	Application(const std::string& window_title)
		: status{SUCCESS}, scale{}, window_title(window_title) {
		// Initialize SDL.
		if (SDL_Init(SDL_INIT_VIDEO) == false) {
			SDL_Log("SDL_Init: %s", SDL_GetError());
			status = INITIALIZATION_ERROR;
			return;
		}

		// Create an SDL window.
		scale = SDL_GetDisplayContentScale(SDL_GetPrimaryDisplay());
		if (scale == 0) {
			scale = 1;	// Use the scaling factor expected by the display based
						// on its DPI settings, default to 1.
		}
		constexpr SDL_WindowFlags flags =
			SDL_WINDOW_HIDDEN | SDL_WINDOW_HIGH_PIXEL_DENSITY |
			SDL_WINDOW_RESIZABLE;  // The window must be shown explicitly.
		window = SDL_CreateWindow(
			window_title.c_str(),
			static_cast<int>(static_cast<float>(1780) * scale), //i think this should automatically adapt to user screen size
			static_cast<int>(static_cast<float>(900) * scale),
			flags);
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

	/**
	Get the status of the application.

	@return the status of the application as an `ApplicationStatus` value.
	*/
	ApplicationStatus get_status() { return status; }

	private:
	void CreateNode() {
		 NodeEditor::Node node;
		 node.id = nodeEditor.getUniqueId();
		 
		 for (int i = 0; i < 2; ++i) {
			NodeImGui::PinId inputPinId = nodeEditor.getUniquePinId();
			node.inputs.push_back(inputPinId);
		 }

		 for (int i = 0; i < 2; ++i) {
			NodeImGui::PinId outputPinId = nodeEditor.getUniquePinId();
			node.outputs.push_back(outputPinId);
		 }

		 nodeEditor.addNode(node);
	}

	// menu for handling the nodestuff. You should be able to select different node types, also handle settings of individual nodes
	void LeftSideMenu() {
		ImGuiViewport* viewport = ImGui::GetMainViewport();
		float menuBarHeight = ImGui::GetFrameHeight();
		ImGui::SetNextWindowPos(ImVec2(viewport->Pos.x, viewport->Pos.y + menuBarHeight));
		ImGui::SetNextWindowSize(ImVec2(viewport->Size.x/4, viewport->Size.y - menuBarHeight));
		ImGui::SetNextWindowViewport(viewport->ID);
		ImGui::Begin("Window A", nullptr,
					 ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoMove |
						 ImGuiWindowFlags_NoResize |
							 ImGuiWindowFlags_NoBringToFrontOnFocus);
		
		
		// RESET BUTTON STARTS HERE
		if (ImGui::Button("Reset")) ImGui::OpenPopup("Reset?");

		// Always center this window when appearing
		ImVec2 center = ImGui::GetMainViewport()->GetCenter();
		ImGui::SetNextWindowPos(center, ImGuiCond_Appearing,
								ImVec2(0.5f, 0.5f));

		if (ImGui::BeginPopupModal("Reset?", NULL,
								   ImGuiWindowFlags_AlwaysAutoResize)) {
			ImGui::Text(
				"The node graph and all related settings will be reset.\nThis operation "
				"cannot be undone.");
			ImGui::Separator();

			// static int unused_i = 0;
			// ImGui::Combo("Combo", &unused_i, "Delete\0Delete harder\0");

			static bool dont_ask_me_next_time = false;
			ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(0, 0));
			ImGui::Checkbox("Don't ask me next time", &dont_ask_me_next_time);
			ImGui::PopStyleVar();

			if (ImGui::Button("OK", ImVec2(120, 0))) {
				ImGui::CloseCurrentPopup();
			}
			ImGui::SetItemDefaultFocus();
			ImGui::SameLine();
			if (ImGui::Button("Cancel", ImVec2(120, 0))) {
				ImGui::CloseCurrentPopup();
			}
			ImGui::EndPopup();
		}
		// RESET BUTTON ENDS HERE


		// OTHER BUTTONS (THEY DO NOT WORK)
		ImGui::SameLine();
		bool butt_2 = ImGui::Button("Load Preset");
		ImGui::SameLine();
		bool butt_3 = ImGui::Button("Save Preset");
		ImGui::SameLine();
		if (ImGui::Button("Focus on canvas")) {
			NodeImGui::NavigateToContent();
		}

		ImGui::Dummy(ImVec2(0.0f, 10.0f));
		// COLAPSING HEADERS START HERE
		if (ImGui::CollapsingHeader("Node handler")) {
			ImGui::SeparatorText("IO Nodes");
			if (ImGui::Button("Add node 1")){
				CreateNode();
			}
			ImGui::SeparatorText("Compressor Nodes");
			bool nodebutt_2 = ImGui::Button("Add node 2");
			ImGui::SeparatorText("IO Nodes");
			bool nodebutt_3 = ImGui::Button("Add node 3");
			ImGui::SeparatorText("Something Something RGB Nodes");
			bool nodebutt_4 = ImGui::Button("Add node 4");
		}
		if (ImGui::CollapsingHeader("Mipmap Options")) {
		}
		if (ImGui::CollapsingHeader("Effects")) {
		}
		if (ImGui::CollapsingHeader("Image Options")) {
		}
		if (ImGui::CollapsingHeader("Compression Settings")) {
		}

		ImGui::Dummy(ImVec2(0.0f, 10.0f)); // cute spacing between drop downs and file loading button
		// THE OG IMAGE MANPIPULATOR WINDOW vvv 
		
		// Testing-window that brings up file explorer
		if (ImGui::Button("File explore tester (also display image)")) {
			printf("Button A clicked!\n");
			SDL_ShowOpenFileDialog(
				callback, renderer, window, dialog_filters.data(),
				SDL_arraysize(dialog_filters), nullptr, true);
		}

		// Show the original images
		if (!original_textures.empty()) {
			for (const auto texture : original_textures) {
				float width, height;  // Width and height are set below
				SDL_GetTextureSize(texture, &width, &height);
				float scaleFactor =  ImGui::GetContentRegionAvail().x / width;
				ImVec2 scaledSize(width * scaleFactor,
								  height * scaleFactor);  // Scale down the image
				ImGui::Image(texture,  scaledSize);
			}
			if (ImGui::Button("Process images (this does not work)")) {
				processImages(renderer);  // Click to manipulate images
			}
		}

		
		ImGui::End();
	}

	// menu for handling "projects"? like saving graphs, and idk. it's the top main menu bar, like the one you usually see in apps
	static void ShowMainMenuBar() {
		if (ImGui::BeginMainMenuBar()) {
			if (ImGui::BeginMenu("File")) {
				ImGui::EndMenu();
			}
			if (ImGui::BeginMenu("Edit")) {
				ImGui::EndMenu();
			}
			if (ImGui::BeginMenu("View")) {
				ImGui::EndMenu();
			}
			if (ImGui::BeginMenu("Help")) {
				ImGui::EndMenu();
			}
			ImGui::EndMainMenuBar();
		}
	}

	void ShowNodeEditor() {

		// Node editor
		ImGuiViewport* viewport = ImGui::GetMainViewport();
		float menuBarHeight = ImGui::GetFrameHeight();
		ImGui::SetNextWindowPos(ImVec2(viewport->Size.x - viewport->Size.x * 3 / 4, viewport->Pos.y + menuBarHeight));
		ImGui::SetNextWindowSize(ImVec2(viewport->Size.x * 3 / 4, viewport->Size.y - menuBarHeight));
		ImGui::SetNextWindowViewport(viewport->ID);
		if (ImGui::Begin("Node Editor", nullptr,ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoBringToFrontOnFocus | ImGuiWindowFlags_NoMove)) {
			ImGui::Text("Node count: %d", nodeEditor.nodes.size());
			ImGui::SameLine();
			ImGui::Text("Current 'unique Pin ID' : %d", nodeEditor.uniquePinId);
			
			nodeEditor.render();  // Render the node editor
			ImGui::End();
		}
	}
	
	public:
	// This function is just for testing and experimenting with ImGui and the
	// node editor. Testing different designs 
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
		style.FontScaleDpi = scale;	 // Set initial font scale.

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
				if (event.type == SDL_EVENT_WINDOW_CLOSE_REQUESTED &&
					event.window.windowID == SDL_GetWindowID(window)) {
					quit = true;
				}
			}

			// Do no rendering if the window is minimized.
			if (SDL_GetWindowFlags(window) & SDL_WINDOW_MINIMIZED) {
				continue;
			}

			// Start of the ImGui frame.
			ImGui_ImplSDLRenderer3_NewFrame();
			ImGui_ImplSDL3_NewFrame();
			ImGui::NewFrame();
			
			// ------ individual window features code here
			ShowNodeEditor();
			ShowMainMenuBar();
			LeftSideMenu();

			// ------ individual window features code here

			// Show demo window.
			ImGui::ShowDemoWindow();

			// Render the ImGui frame.
			ImGui::Render();
			SDL_SetRenderScale(renderer, io.DisplayFramebufferScale.x,io.DisplayFramebufferScale.y);
			SDL_SetRenderDrawColorFloat(renderer, 0, 0, 0, 0);
			SDL_RenderClear(renderer);
			ImGui_ImplSDLRenderer3_RenderDrawData(ImGui::GetDrawData(),
													renderer);
			SDL_RenderPresent(renderer);
		}
	}

	~Application() {
		// ImGui cleanup.
		nodeEditor.cleanup();
		ImGui_ImplSDLRenderer3_Shutdown();
		ImGui_ImplSDL3_Shutdown();
		ImGui::DestroyContext();

		/*
		SDL cleanup.

		If the application is not properly initialized, e.g. due to an error,
		some fields may be null and that case must be handled properly.

		There might be a cleaner way to do this, but we are not really
		initializing that much stuff so it probably does not matter.
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
	Application application = Application(
		"DD1367 Compression & Texturepacking Node Editor :3");

	// Check for initialization errors before running.
	if (application.get_status() != SUCCESS) {
		return application.get_status();
	}

	application.run();

	return application.get_status();
}