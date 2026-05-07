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
		NodeImGui::NodeId spawnNodeId = 0;
		struct Link {
			NodeImGui::LinkId id;
			NodeImGui::PinId startPin;
			NodeImGui::PinId endPin;
		};

		class Node {
		    public:
			NodeImGui::NodeId id;
			ImVector<NodeImGui::PinId> inputs;
			ImVector<NodeImGui::PinId> outputs;
			SDL_Texture* texture =
				{};	 // the image that is being processed by this node (should
					 // update between nodes?)
			bool isInputNode = false;
			bool isOutputNode = false;

			void setTexture(SDL_Texture* newTexture) { texture = newTexture; }

			virtual void process() {
				// implement specific processing logic for different nodes
			}
		};

		class InputNode : public Node {
			void process() {
			}
		};

		class OutputNode : public Node {

			void process() {}
		};

		class MipMapNode : public Node {
			int mipmapLevels;
			void process() {
			
			}
		};

		class EffectNode : public Node {
			enum EffectType {
				TWOD_Convolution, 
				Negative,
				Lighter,
				Darker,
				Contrast_More,
				Contrast_Less,
				Smooth,
				Sharpen_Soft,
				Sharpen_Medium,
				Sharpen_Strong,
				Find_Edges,
				Contour,
				Edge_Detect,
				Edge_Detect_Soft,
				Emboss,
				Gaussian_Blur,
				Adjust_Contrast,
				Unsharp_Mask,
				Super_Resolution,
				sRGB_to_Linear,
				Linear_to_sRGB,
				Edge_Pad,
				Resize,
				Swizzle
			};
			EffectType effectType;

			// now depending on what effecttype a specific node object has, perform different operations on the image
			void process() {
				switch (effectType) {
					case TWOD_Convolution:
						// perform 2D convolution on the image
						break;
					case Negative:
						// perform negative effect on the image
						break;
					case Lighter:
						// perform lighter effect on the image
						break;
					case Darker:
						// perform darker effect on the image
						break;
					case Contrast_More:
						// perform contrast more effect on the image
						break;
					case Contrast_Less:
						// perform contrast less effect on the image
						break;
					case Smooth:
						// perform smooth effect on the image
						break;
					case Sharpen_Soft:
						// perform sharpen soft effect on the image
						break;
					case Sharpen_Medium:
						// perform sharpen medium effect on the image
						break;
					case Sharpen_Strong:
						// perform sharpen strong effect on the image
						break;
					case Find_Edges:
						// perform find edges effect on the image
						break;
					case Contour:
						// perform contour effect on the image
						break;
					case Edge_Detect:
						// perform edge detect effect on the image
						break;
					case Edge_Detect_Soft:
						// perform edge detect soft effect on the image
						break;
					case Emboss:
						// perform emboss effect on the image
						break;
					case Gaussian_Blur:
						// perform gaussian blur effect on the image
						break;
					case Adjust_Contrast:
						// perform adjust contrast effect on the image
						break;
					case Unsharp_Mask:
						// perform unsharp mask effect on the image
						break;
					case Super_Resolution:
						// perform super resolution effect on the image
						break;
					case sRGB_to_Linear:
						// perform sRGB to linear effect on the image
						break;
					case Linear_to_sRGB:
						// perform linear to sRGB effect on the image
						break;
					case Edge_Pad:
						// perform edge pad effect on the image
						break;
					case Resize:
						// perform resize effect on the image
						break;
					case Swizzle:
						// perform swizzle effect on the image
						break;
					default:
						// handle errors ig
						break;
				}
			}
		};

		class CompressionNode : public Node {
			enum CompressionType {
				BC7,
				BC6S, 
				ASTC,
				BC3,
				EIGHT,
				USTC,
				BC1
				//etc? har begränsat kunskap om vad som är rimligt här
			};
			CompressionType compressionType;

			void process() {
				switch (compressionType) {
					case BC7:
						break;
					case BC6S:
						break;
					case ASTC:
						break;
					case BC3:
						break;
					case EIGHT:
						break;
					case USTC:
						break;
					case BC1:
						break;
					default:
						break;
				}
			}
		};

		NodeImGui::NodeId getUniqueId() {
			return NodeImGui::NodeId(uniqueId++);
		}

		NodeImGui::PinId getUniquePinId() {
			return NodeImGui::PinId(uniqueId++);
		}

		NodeEditor() {
			NodeImGui::Config config;
			config.SettingsFile = "NodeEditor.json";
			nodeContext = NodeImGui::CreateEditor(&config);
		}

		std::vector<Node> nodes;
		std::vector<Link> links;

		void addNode(const Node& node) { nodes.push_back(node); }

		void addLink(const Link& link) { links.push_back(link); }

		void removeNode(NodeImGui::NodeId nodeToDelete) {
			const auto id = std::find_if(
				nodes.begin(), nodes.end(),
				[nodeToDelete](auto& node) { return node.id == nodeToDelete; });
			if (id != nodes.end()) {
				nodes.erase(id);
			}
		}

		void removeLink(NodeImGui::LinkId linkToDelete) {
			const auto id = std::find_if(
				links.begin(), links.end(),
				[linkToDelete](auto& link) { return link.id == linkToDelete; });
			if (id != links.end()) {
				links.erase(id);
			}
		}

		bool linkExists(NodeImGui::PinId startPin, NodeImGui::PinId endPin) {
			return std::any_of(
				links.begin(), links.end(),
				[startPin, endPin](const Link& link) {
					return (link.startPin == startPin && link.endPin == endPin) ||
						   (link.startPin == endPin && link.endPin == startPin);
				});
		}

		void cleanup() const { NodeImGui::DestroyEditor(nodeContext); }

		void render(Application* a) {
			NodeImGui::SetCurrentEditor(nodeContext);
			NodeImGui::Begin("Node Editor");

			for (auto& node : nodes) {
				NodeImGui::BeginNode(node.id);

				ImGui::Text("Node - ID: %lu", node.id.Get());

				for (const auto& inputPin : node.inputs) {
					NodeImGui::BeginPin(inputPin, NodeImGui::PinKind::Input);
					ImGui::Text("-> In");
					NodeImGui::EndPin();
				}
				for (const auto& outputPin : node.outputs) {
					NodeImGui::BeginPin(outputPin, NodeImGui::PinKind::Output);
					ImGui::Text("Out ->");
					NodeImGui::EndPin();
				}

				// problem med att node-imagen bara uppdaterar om man trycker på load-image igen efter man loadat
				if (node.isInputNode) {
					//pop and push unique IDs
					if (ImGui::Button("Import Image")) {
						SDL_ShowOpenFileDialog(
							callback, a->renderer, a->window, dialog_filters.data(),
							SDL_arraysize(dialog_filters), nullptr, true);
					}
					if (!original_textures.empty()) {
						if (ImGui::Button("Load")) { // Right now you HAVE to load the image you imported, otherwise it won't be saved in the node. This could be worked around if you do node-saving in the callback, but I didn't feel like doing that.
							node.setTexture(original_textures.back());
							original_textures
								.pop_back();
						}
					}
					if (node.texture != NULL) {
						float width, height;
						SDL_GetTextureSize(node.texture, &width, &height);
						float scaleFactor =
							ImGui::GetContentRegionAvail().x / width;
						ImVec2 scaledSize(
							width * scaleFactor,
							height * scaleFactor);	// Scale down the image
						ImGui::Image(node.texture, scaledSize);
					}
				}

				if (node.isOutputNode) {
					if (ImGui::Button("Process chain")) {
						//node.process(); ?
					}
					if (node.texture!=nullptr) {
						float width, height;
						SDL_GetTextureSize(node.texture, &width,
										   &height);
						float scaleFactor =
							ImGui::GetContentRegionAvail().x / width;
						ImVec2 scaledSize(
							width * scaleFactor,
							height * scaleFactor);	// Scale down the image
						ImGui::Image(node.texture, scaledSize);
					}
				}

				for (const auto& link : links) {
					NodeImGui::Link(link.id, link.startPin, link.endPin);
				}

				NodeImGui::EndNode();
			}

			// Create new links between pins
			if (NodeImGui::BeginCreate()) {
				NodeImGui::PinId source, destination;
				NodeImGui::NodeId sourceNode, destinationNode;
				if (NodeImGui::QueryNewLink(&source, &destination)) {
					if (source && destination && NodeImGui::AcceptNewItem() && !linkExists(source, destination)) {
						bool sourceIsInput = false;
						bool destinationIsOutput = false;
						bool sourceIsOutput = false;
						bool destinationIsInput = false;
						bool sameNode = false;
						bool valid = false;
						bool numberOfLinksIsLessThanZero =
							true;  // tvinga eventuellt 1 link per pin? (ej
								   // implementerat än)
						Link link;
						link.id = uniqueId++;
						// make sure output pin actually goes into an input pin
						// or vice versa (forbid input->input and
						// output->output)
						for (const auto& node : nodes) {
							if (node.inputs.contains(source)) {
								sourceIsInput = true;
								sourceNode = node.id;
							}
							if (node.outputs.contains(destination)) {
								destinationIsOutput = true;
								destinationNode = node.id;
							}
							if (node.outputs.contains(source)) {
								sourceIsOutput = true;
								sourceNode = node.id;
							}
							if (node.inputs.contains(destination)) {
								destinationIsInput = true;
								destinationNode = node.id;
							}
						}

						if ((sourceIsInput && destinationIsOutput)) {
							link.startPin = source;
							link.endPin = destination;
							valid = true;
						} else if (sourceIsOutput && destinationIsInput) {
							valid = true;
							link.endPin = source;
							link.startPin = destination;
						}

						if (valid && (sourceNode!=destinationNode)) {
							printf("Link Created: %d -> %d with ID %d \n",
								   static_cast<int>(link.startPin.Get()),
								   static_cast<int>(link.endPin.Get()),
								   static_cast<int>(link.id.Get()));
							links.push_back(link);
						}
					}
				}
				NodeImGui::EndCreate();
			}

			// Delete links and nodes
			if (NodeImGui::BeginDelete()) {
				NodeImGui::LinkId linkId;
				while (NodeImGui::QueryDeletedLink(&linkId)) {
					if (NodeImGui::AcceptDeletedItem()) {
						removeLink(linkId);
					}
				}
				NodeImGui::NodeId nodeId = 0;
				while (NodeImGui::QueryDeletedNode(&nodeId)) {
					if (NodeImGui::AcceptDeletedItem()) {
						removeNode(nodeId);
					}
				}
				NodeImGui::EndDelete();
			}

			NodeImGui::End();
		}

		void parseLinksToFindPathBetweenInputAndOutput(NodeImGui::NodeId inputNodeId, NodeImGui::NodeId outputNodeId) {
			// Implement a graph traversal algorithm (like BFS or DFS) to find a path
			// between the input node and the output node using the links.
			// This is a placeholder for the actual implementation.
		}

		void reset() {
			nodes.clear();
			links.clear();
			uniqueId = 1;
		}
	};

	NodeEditor nodeEditor;

   public:
	/**
	Initialize the application with the provided window dimensions and title.

	@return An `Application` object. Make sure to call `get_status()` on the
	returned object before using it to find out if initialization has failed!
	*/
	explicit Application(const std::string& window_title)
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
			static_cast<int>(static_cast<float>(1780) *
							 scale),  // I think this should automatically adapt
									  // to user screen size
			static_cast<int>(static_cast<float>(900) * scale), flags);
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
	[[nodiscard]] ApplicationStatus get_status() const { return status; }

   private:
	// Function to create a node with 2 input pins and 2 outputs, has no
	// specific type, just a generic node for testing
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

	void CreateInputNode() {
		NodeEditor::InputNode node;
		node.id = nodeEditor.getUniqueId();
		NodeImGui::PinId outputPinId = nodeEditor.getUniquePinId();
		node.outputs.push_back(outputPinId);
		node.isInputNode = true;
		nodeEditor.addNode(node);
	}

	void CreateOutputNode() {
		NodeEditor::OutputNode node;
		node.id = nodeEditor.getUniqueId();
		NodeImGui::PinId inputPinId = nodeEditor.getUniquePinId();
		node.inputs.push_back(inputPinId);
		node.isOutputNode = true;
		nodeEditor.addNode(node);
	}

	static void HelpMarker(const char* desc) {
		ImGui::TextDisabled("(?)");
		if (ImGui::BeginItemTooltip()) {
			ImGui::PushTextWrapPos(ImGui::GetFontSize() * 35.0f);
			ImGui::TextUnformatted(desc);
			ImGui::PopTextWrapPos();
			ImGui::EndTooltip();
		}
	}

	// menu for handling the node-stuff. You should be able to select different
	// node types, also handle settings of individual nodes
	void LeftSideMenu() {
		const ImGuiViewport* viewport = ImGui::GetMainViewport();
		const float menuBarHeight = ImGui::GetFrameHeight();
		ImGui::SetNextWindowPos(
			ImVec2(viewport->Pos.x, viewport->Pos.y + menuBarHeight));
		ImGui::SetNextWindowSize(
			ImVec2(viewport->Size.x / 4, viewport->Size.y - menuBarHeight));
		ImGui::SetNextWindowViewport(viewport->ID);
		ImGui::Begin("Window A", nullptr,
					 ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoMove |
						 ImGuiWindowFlags_NoResize |
						 ImGuiWindowFlags_NoBringToFrontOnFocus);

		// RESET BUTTON STARTS HERE
		if (ImGui::Button("Reset")) ImGui::OpenPopup("Reset?");

		// Always center this window when appearing
		const ImVec2 center = ImGui::GetMainViewport()->GetCenter();
		ImGui::SetNextWindowPos(center, ImGuiCond_Appearing,
								ImVec2(0.5f, 0.5f));

		if (ImGui::BeginPopupModal("Reset?", nullptr,
								   ImGuiWindowFlags_AlwaysAutoResize)) {
			ImGui::Text(
				"The node graph and all related settings will be reset.\nThis "
				"operation "
				"cannot be undone.");
			ImGui::Separator();
			static bool dont_ask_me_next_time = false;
			ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(0, 0));
			ImGui::Checkbox("Don't ask me next time", &dont_ask_me_next_time);
			ImGui::PopStyleVar();

			if (ImGui::Button("OK", ImVec2(120, 0))) {
				nodeEditor.reset();
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
		if (ImGui::Button("Center on graph")) {
			NodeImGui::NavigateToContent();
		}

		ImGui::Dummy(ImVec2(0.0f, 10.0f));
		// COLLAPSING HEADERS START HERE
		if (ImGui::CollapsingHeader("IO")) {
			ImGui::Dummy(ImVec2(0.0f, 10.0f));
			if (ImGui::Button("Create Input Node")) {
				CreateInputNode();
			}
			ImGui::SameLine();
			HelpMarker(
				"This node will be used to load images into the graph. You can "
				"load multiple images at once, and they will be processed in "
				"parallel.");
			ImGui::Dummy(ImVec2(0.0f, 10.0f));
			if (ImGui::Button("Create Output Node")) {
				CreateOutputNode();
			}
			ImGui::SameLine();
			HelpMarker(
				"This node will be used to view the processed images. You can "
				"save multiple images at once, and they will be processed in "
				"parallel.");
			ImGui::Dummy(ImVec2(0.0f, 10.0f));

			ImGui::SeparatorText("Options");
			// vet inte riktigt vad som ska vara här
		}
		if (ImGui::CollapsingHeader("Mipmap")) {
			ImGui::Dummy(ImVec2(0.0f, 10.0f));
			if (ImGui::Button("Create MipMap Node")) {
			}
			ImGui::SameLine();
			HelpMarker(
				"This node will be used to generate mipmaps for the input "
				"images. "
				"You can choose the number of mipmap levels to generate, and "
				"the "
				"filtering method to use.");
			ImGui::SeparatorText("Options");
		}
		if (ImGui::CollapsingHeader("Effects")) {
			ImGui::Dummy(ImVec2(0.0f, 10.0f));
			ImGui::BeginChild("ChildR", ImVec2(0, 160));
			if (ImGui::BeginTable("Effects' table", 1)) {
				ImGui::TableNextColumn();

				if (ImGui::Button("2D Convolution")) {
				}

				if (ImGui::Button("Negative")) {
				}

				if (ImGui::Button("Lighter")) {
				}

				if (ImGui::Button("Darker")) {
				}

				if (ImGui::Button("Contrast More")) {
				}

				if (ImGui::Button("Contrast Less")) {
				}

				if (ImGui::Button("Smooth")) {
				}

				if (ImGui::Button("Sharpen Soft")) {
				}

				if (ImGui::Button("Sharpen Medium")) {
				}

				if (ImGui::Button("Sharpen Strong")) {
				}

				if (ImGui::Button("Find Edges")) {
				}

				if (ImGui::Button("Contour")) {
				}

				if (ImGui::Button("Edge Detect")) {
				}

				if (ImGui::Button("Edge Detect Soft")) {
				}

				if (ImGui::Button("Emboss")) {
				}

				if (ImGui::Button("Gaussian Blur")) {
				}

				if (ImGui::Button("Adjust Contrast")) {
				}

				if (ImGui::Button("Unsharp Mask")) {
				}

				if (ImGui::Button("Super-Resolution")) {
				}

				if (ImGui::Button("sRGB to Linear")) {
				}

				if (ImGui::Button("Linear to sRGB")) {
				}

				if (ImGui::Button("Edge Pad (Solidify)")) {
				}

				if (ImGui::Button("Resize")) {
				}

				if (ImGui::Button("Swizzle")) {
				}

				ImGui::EndTable();
			}
			ImGui::EndChild();
			ImGui::Dummy(ImVec2(0.0f, 10.0f));
		}
		if (ImGui::CollapsingHeader("Compression Settings")) {
			ImGui::Dummy(ImVec2(0.0f, 10.0f));
			ImGui::BeginChild("ChildC", ImVec2(0, 160));
			if (ImGui::BeginTable("Compression table", 1)) {
				ImGui::TableNextColumn();
				if (ImGui::Button("BC7")) {
				}
				if (ImGui::Button("BC6S")) {
				}
				if (ImGui::Button("ASTC")) {
				}

				if (ImGui::Button("BC3")) {
				}

				if (ImGui::Button("BC1")) {
				}

				if (ImGui::Button("8")) {
				}

				if (ImGui::Button("USTC")) {
				}

				if (ImGui::Button("Och så vidare..")) {
				}

				ImGui::EndTable();
				ImGui::EndChild();
				ImGui::Dummy(ImVec2(0.0f, 10.0f));
			}
		}
		if (ImGui::CollapsingHeader("Image Options")) {
			ImGui::Dummy(ImVec2(0.0f, 10.0f));
			if (ImGui::Button("Color Map")) {
			}
			ImGui::SameLine();
			HelpMarker(
				"Saves the image as a color texture; does not convert to "
				"grayscale or normal map.");
			if (ImGui::Button("Grayscale")) {
			}
			ImGui::SameLine();
			HelpMarker("Converts the image to grayscale");
			if (ImGui::Button("Normal Map: Tangent Space")) {
			}
			ImGui::SameLine();
			HelpMarker(
				"Converts the image to a tangent-space normal map: exports "
				"the normal vector (x y z) as the color (0.5*x + 0.5, "
				"0.5*y + 0.5 , 0.5*z + 0.5)");
			if (ImGui::Button("Normal Map: Object Space")) {
			}
			ImGui::SameLine();
			HelpMarker(
				"Converts the image to an object-space normal map: exports "
				"the normal vector (x y z) as the color (saknas text) and "
				"can apply cube map coordinate space onversion if the "
				"image is a cube map");
		}

		ImGui::Dummy(ImVec2(0.0f, 10.0f));
		ImGui::SeparatorText("Testing stuff below");

		ImGui::Dummy(ImVec2(
			0.0f,
			10.0f));  // cute spacing between drop-downs and file loading button

		ImGui::Dummy(ImVec2(0.0f, 10.0f));
		if (ImGui::Button("Add node for testing")) {
			CreateNode();
		}

		ImGui::End();
	}

	// menu for handling "projects"? like saving graphs, and IDK. it's the top
	// main menu bar, like the one you usually see in apps
	static void ShowMainMenuBar() {
		if (ImGui::BeginMainMenuBar()) {
			if (ImGui::BeginMenu("File")) {
				ImGui::EndMenu();
			}

			if (ImGui::BeginMenu("View")) {
				if (ImGui::BeginMenu("Theme")) {
					if (ImGui::MenuItem("Classic")) {
						ImGui::StyleColorsClassic();
					};
					if (ImGui::MenuItem("Light")) {
						ImGui::StyleColorsLight();
					};
					if (ImGui::MenuItem("Dark")) {
						ImGui::StyleColorsDark();
					};
					ImGui::EndMenu();
				}
				ImGui::EndMenu();
			}

			if (ImGui::BeginMenu("Help")) {
				ImGui::MenuItem("Github: ");
				// fixa länk typ?
				ImGui::EndMenu();
			}
			ImGui::EndMainMenuBar();
		}
	}

	void ShowNodeEditor() {
		// Node editor
		const ImGuiViewport* viewport = ImGui::GetMainViewport();
		const float menuBarHeight = ImGui::GetFrameHeight();
		ImGui::SetNextWindowPos(
			ImVec2(viewport->Size.x - viewport->Size.x * 3 / 4,
				   viewport->Pos.y + menuBarHeight));
		ImGui::SetNextWindowSize(
			ImVec2(viewport->Size.x * 3 / 4, viewport->Size.y - menuBarHeight));
		ImGui::SetNextWindowViewport(viewport->ID);
		if (ImGui::Begin("Node Editor", nullptr,
						 ImGuiWindowFlags_NoTitleBar |
							 ImGuiWindowFlags_NoResize |
							 ImGuiWindowFlags_NoBringToFrontOnFocus |
							 ImGuiWindowFlags_NoMove)) {
			HelpMarker(
				"Tip: \n Press DELETE KEY to remove nodes \n Press F to "
				"center on graph");
			ImGui::SameLine();
			ImGui::Text("          Node count: %lu", nodeEditor.nodes.size());
			if (ImGui::IsMousePosValid()) {
				ImGui::SameLine();
				ImGui::Text("             Mouse pos: (%g, %g)",
							ImGui::GetIO().MousePos.x,
							ImGui::GetIO().MousePos.y);
			}
			nodeEditor.render(this);  // Render the node editor
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
			SDL_SetRenderScale(renderer, io.DisplayFramebufferScale.x,
							   io.DisplayFramebufferScale.y);
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
	auto application =	// Replace text once we have a real product name
		Application("DD1367 Compression & Texture-packing Node Editor");

	// Check for initialization errors before running.
	if (application.get_status() != SUCCESS) {
		return application.get_status();
	}

	application.run();

	return application.get_status();
}