// Because the application is providing its own entry point.
#define SDL_MAIN_HANDLED 0x39  // NOLINT

#include <SDL3/SDL.h>
#include <SDL3/SDL_dialog.h>
#include <SDL3/SDL_main.h>
#include <SDL3_image/SDL_image.h>
#include <backends/imgui_impl_sdl3.h>
#include <backends/imgui_impl_sdlrenderer3.h>
#include <bc7decomp.h>
#include <imgui.h>
#include <imgui_node_editor.h>
#include <libbc7enc.h>
#include <rgbcx.h>
#include <utils.h>
#include <rdo_bc_encoder.h>

#include <algorithm>
#include <array>
#include <fstream>
#include <memory>
#include <stack>
#include <string>
#include <unordered_set>
#include <unordered_map>
#include <vector>
#include <filesystem>
namespace fs = std::filesystem;
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
constexpr std::array<SDL_DialogFileFilter, 3> dialog_filters = {
	SDL_DialogFileFilter{"PNG images", "png"},
	SDL_DialogFileFilter{"JPEG images", "jpg;jpeg"},
	SDL_DialogFileFilter{"All images", "png;jpg;jpeg"}};

std::vector<std::string> selected_files;  // Vector (aka C++ ArrayList) to store
										  // the files selected by the user
// Store unprocessed textures
std::vector<SDL_Texture*> original_textures;

// Callback function used to bring up file explorer dialog
static void SDLCALL load_callback(void* userdata, const char* const* filelist,
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

std::string selected_folder;
static void SDLCALL folder_callback(void* userdata, const char* const* foldername, const int filter) {
	if (!foldername) {
		SDL_Log("An error occurred: %s", SDL_GetError());
		return;
	} else if (!*foldername) {
		SDL_Log("The user did not select any folder.");
		SDL_Log("Most likely, the dialog was canceled.");
		return;
	}
	selected_folder = *foldername;
	selected_folder += "/";
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

			SDL_Texture* texture = nullptr;
			// Uncompressed pixel surface associated with this node (if any).
			SDL_Surface* surface = nullptr;
			std::string sourceFile;
			std::string outputFile;
			// Compressed data produced by compression nodes (raw block data).
			std::vector<char> compressedBlocks;
			int compressedWidth = 0;
			int compressedHeight = 0;
			int compressedBytesPerBlock = 0;
			int compressedNumBlocks = 0;
			int compressedFormat = 0;
			bool isInputNode = false;
			bool isOutputNode = false;

			virtual ~Node() {
				clearOutputSurfaces();

				if (texture) {
					SDL_DestroyTexture(texture);
				}

				if (surface) {
					SDL_DestroySurface(surface);
				}
			}

			enum class PinDataKind { Image, Red, Green, Blue, Alpha };

			struct PinInfo {
				NodeImGui::PinId id;
				PinDataKind kind;
			};

			std::unordered_map<uintptr_t, SDL_Surface*> outputSurfaces;
			std::unordered_map<uintptr_t, PinDataKind> outputPinKinds;

			void clearOutputSurfaces() {
				for (auto& [_, surface] : outputSurfaces) {
					if (surface) {
						SDL_DestroySurface(surface);
					}
				}

				outputSurfaces.clear();
			}

			PinDataKind getOutputKind(NodeImGui::PinId pin) const {
				auto it = outputPinKinds.find(pin.Get());

				if (it != outputPinKinds.end()) {
					return it->second;
				}

				return PinDataKind::Image;
			}

			[[nodiscard]] virtual const char* name() const { return "Node"; }

			[[nodiscard]] virtual const char* getInputLabel(
				NodeImGui::PinId pin) const {
				return "In";
			}

			[[nodiscard]] virtual const char* getOutputLabel(
				NodeImGui::PinId pin) const {
				return "Out";
			}

			virtual void renderBody(Application* app, NodeEditor& editor) {
				// Default node has no custom UI.
			}

			// Process the node. The pipeline maintains an in-memory
			// `currentSurface` that is passed by reference between nodes. Nodes
			// may read and/or replace `currentSurface`. Compression nodes
			// should populate `compressedBlocks` but may leave `currentSurface`
			// intact for downstream preview/processing.
			virtual bool process(NodeEditor& editor, SDL_Renderer* renderer,
								 Node* inputNode,
								 SDL_Surface*& currentSurface, Application* app) {
				return true;
			}
		};

		// Structure that keeps track of a link's source and destination Node
		struct GraphEdge {
			Node* source;
			Node* destination;
		};

		class RGBASplitNode : public Node {
		   public:
			NodeImGui::PinId redOutput;
			NodeImGui::PinId greenOutput;
			NodeImGui::PinId blueOutput;
			NodeImGui::PinId alphaOutput;

			const char* name() const override { return "RGBA Split"; }

			const char* getOutputLabel(NodeImGui::PinId pin) const override {
				if (pin == redOutput) return "R";
				if (pin == greenOutput) return "G";
				if (pin == blueOutput) return "B";
				if (pin == alphaOutput) return "A";

				return "Out";
			}

			bool process(NodeEditor& editor, SDL_Renderer* renderer,
						 Node* inputNode, SDL_Surface*& currentSurface,
						 Application* app) override {
				if (!currentSurface) {
					SDL_Log("RGBA Split node has no current surface.");
					return false;
				}

				utils::image_u8 img;

				if (!editor.surfaceToImageU8(currentSurface, img)) {
					SDL_Log(
						"RGBA Split failed converting surface to image_u8.");
					return false;
				}

				clearOutputSurfaces();

				outputSurfaces[redOutput.Get()] =
					makeChannelSurface(img, PinDataKind::Red);

				outputSurfaces[greenOutput.Get()] =
					makeChannelSurface(img, PinDataKind::Green);

				outputSurfaces[blueOutput.Get()] =
					makeChannelSurface(img, PinDataKind::Blue);

				outputSurfaces[alphaOutput.Get()] =
					makeChannelSurface(img, PinDataKind::Alpha);

				if (texture) {
					SDL_DestroyTexture(texture);
					texture = nullptr;
				}

				texture =
					SDL_CreateTextureFromSurface(renderer, currentSurface);

				return true;
			}

			private:
			static SDL_Surface* makeChannelSurface(const utils::image_u8& img,
												   PinDataKind channel) {
				SDL_Surface* out = SDL_CreateSurface(
					img.width(), img.height(), SDL_PIXELFORMAT_RGBA32);

				if (!out) {
					SDL_Log("Failed creating channel surface: %s",
							SDL_GetError());
					return nullptr;
				}

				uint8_t* dst = static_cast<uint8_t*>(out->pixels);

				for (uint32_t y = 0; y < img.height(); ++y) {
					for (uint32_t x = 0; x < img.width(); ++x) {
						const utils::color_quad_u8 c = img(x, y);

						uint8_t v = 0;

						switch (channel) {
							case PinDataKind::Red:
								v = c.r;
								break;
							case PinDataKind::Green:
								v = c.g;
								break;
							case PinDataKind::Blue:
								v = c.b;
								break;
							case PinDataKind::Alpha:
								v = c.a;
								break;
							default:
								v = 0;
								break;
						}

						uint8_t* p = dst + y * out->pitch + x * 4;

						p[0] = v;
						p[1] = v;
						p[2] = v;
						p[3] = 255;
					}
				}

				return out;
			}
		};

		SDL_Surface* getLinkedSurfaceForInput(NodeImGui::PinId inputPin) const {
			for (const auto& link : links) {
				if (link.endPin == inputPin) {
					Node* upstream = findNodeOwningPin(link.startPin);

					if (!upstream) return nullptr;

					auto it =
						upstream->outputSurfaces.find(link.startPin.Get());

					if (it != upstream->outputSurfaces.end()) {
						return it->second;	// split channel output
					}

					if (upstream->surface) {
						return upstream->surface;  // full image input
					}

					return nullptr;
				}
			}

			return nullptr;
		}

		class RGBAToNewSurfaceNode : public Node {
		   public:
			NodeImGui::PinId redInput;
			NodeImGui::PinId greenInput;
			NodeImGui::PinId blueInput;
			NodeImGui::PinId alphaInput;
			NodeImGui::PinId imageOutput;

			const char* name() const override { return "RGBA -> New Surface"; }

			const char* getInputLabel(NodeImGui::PinId pin) const override {
				if (pin == redInput) return "R";
				if (pin == greenInput) return "G";
				if (pin == blueInput) return "B";
				if (pin == alphaInput) return "A";

				return "In";
			}

			const char* getOutputLabel(NodeImGui::PinId pin) const override {
				if (pin == imageOutput) return "RGBA";

				return "Out";
			}

			bool process(NodeEditor& editor, SDL_Renderer* renderer,
						 Node* inputNode, SDL_Surface*& currentSurface,
						 Application* app) override {
				SDL_Surface* r = editor.getLinkedSurfaceForInput(redInput);
				SDL_Surface* g = editor.getLinkedSurfaceForInput(greenInput);
				SDL_Surface* b = editor.getLinkedSurfaceForInput(blueInput);
				SDL_Surface* a = editor.getLinkedSurfaceForInput(alphaInput);
				
				/*
				if (!r && !g && !b) {
					SDL_Log(
						"RGBA -> New Texture requires at least one R/G/B "
						"inputs.");
					return false;
				}
				*/
				
				SDL_Surface* result = combineChannels(r, g, b, a);

				if (!result) {
					SDL_Log("RGBA -> New Texture failed combining channels.");
					return false;
				}

				if (currentSurface) {
					SDL_DestroySurface(currentSurface);
				}

				currentSurface = result;

				if (texture) {
					SDL_DestroyTexture(texture);
					texture = nullptr;
				}

				texture =
					SDL_CreateTextureFromSurface(renderer, currentSurface);


				sourceFile = inputNode->sourceFile;
				outputFile.clear();	 // Clear output file since this is an
									 // intermediate node


				return true;
			}

		   private:
			static uint8_t readGray(SDL_Surface* surface, int x, int y) {
				uint8_t* pixels = static_cast<uint8_t*>(surface->pixels);
				uint8_t* p = pixels + y * surface->pitch + x * 4;
				return p[0];
			}

			static SDL_Surface* convertToRGBA32(SDL_Surface* surface) {
				if (!surface) return nullptr;
				SDL_Surface* converted =
					SDL_ConvertSurface(surface, SDL_PIXELFORMAT_RGBA32);
				if (!converted) {
					SDL_Log("Failed converting surface to RGBA32: %s",
							SDL_GetError());
				}
				return converted;
			}

			static SDL_Surface* combineChannels(SDL_Surface* r, SDL_Surface* g,
												SDL_Surface* b,
												SDL_Surface* a) {

				const int w =
					r ? r->w : (g ? g->w : (b ? b->w : (a ? a->w : 1)));

				const int h =
					r ? r->h : (g ? g->h : (b ? b->h : (a ? a->h : 1)));

				SDL_Surface* rr = convertToRGBA32(r);
				SDL_Surface* gg = convertToRGBA32(g);
				SDL_Surface* bb = convertToRGBA32(b);
				SDL_Surface* aa = convertToRGBA32(a);
				SDL_Surface* result =
					SDL_CreateSurface(w, h, SDL_PIXELFORMAT_RGBA32);

				if (!result) return nullptr;

				for (int y = 0; y < h; ++y) {
					for (int x = 0; x < w; ++x) {
						uint8_t rv = rr ? readGray(rr, x, y) : 0;
						uint8_t gv = gg ? readGray(gg, x, y) : 0;
						uint8_t bv = bb ? readGray(bb, x, y) : 0;
						uint8_t av = aa ? readGray(aa, x, y) : 255;
						

						uint8_t* pixels = static_cast<uint8_t*>(result->pixels);
						uint8_t* p = pixels + y * result->pitch + x * 4;

						p[0] = rv;
						p[1] = gv;
						p[2] = bv;
						p[3] = av;
					}
				}

				return result;
			}
		};

		class InputNode : public Node {
			bool containsImage = false;

		   public:
			[[nodiscard]] const char* name() const override {
				return "Input Node";
			}
			InputNode() { this->containsImage = false; }
			std::string filename;
			void renderBody(Application* app, NodeEditor& editor) override {
				if (ImGui::Button("Import Image")) {
					SDL_ShowOpenFileDialog(load_callback, app->renderer, app->window,
										   dialog_filters.data(),
										   SDL_arraysize(dialog_filters),
										   nullptr, true);
					this->containsImage = true;
				}

				if (!original_textures.empty() && !selected_files.empty() &&
					containsImage) {
					if (ImGui::Button("Load")) {
						texture = original_textures.back();
						// Try to also load the underlying surface for in-memory
						// processing IMG_Load returns an SDL_Surface*, but we
						// previously loaded textures. Load a surface here from
						// the path instead.
						surface = IMG_Load(selected_files.back().c_str());

						if (!surface) {
							SDL_Log("IMG_Load(surface) failed: %s",
									SDL_GetError());
						}

						sourceFile = selected_files.back();
						fs::path path(sourceFile);
						filename = path.filename().string();
						

						original_textures.pop_back();
						selected_files.pop_back();
						this->containsImage = false;
					}
				}

				if (!sourceFile.empty()) {
					ImGui::Text("%s", filename.c_str());
				}

				if (texture != nullptr) {
					float width, height;
					SDL_GetTextureSize(texture, &width, &height);

					const float maxWidth =
						150.0f / width;  // Scale down if texture is wider than
										  // 150 pixels
					const float scaleFactor = std::min(maxWidth, 1.0f);

					const ImVec2 scaledSize(width * scaleFactor,
											height * scaleFactor);

					ImGui::Image(texture, scaledSize);
				}
			}
		};

		class OutputNode : public Node {
			bool containsImage = false;
			std::vector<Node*> processingPath;
			Node* inputNode = nullptr;
			bool fallbackToDisk = false;
			bool generateMipmaps = false;
			bool outputfileSet = false;

		   public:
			OutputNode() { this->containsImage = false; }
			[[nodiscard]] const char* name() const override {
				return "Output Node";
			}

			void renderBody(Application* app, NodeEditor& editor) override {
				ImGui::Checkbox("Generate mipmaps", &generateMipmaps);
				if (selected_folder.empty()) {
					if (ImGui::Button("Select folder")) {
						SDL_ShowOpenFolderDialog(folder_callback, nullptr,
												 app->window, nullptr, false);
					}
				} else if (ImGui::Button("Process chain")) {
					processingPath =
						editor.findPathBetweenInputAndOutput(this->id);

					if (!processingPath.empty()) {
						inputNode = processingPath.front();
						sourceFile = inputNode->sourceFile;

						SDL_Log("Found path with %lu nodes:",
								processingPath.size());
						SDL_Log("Input node: %s (ID: %lu)", inputNode->name(),
								inputNode->id.Get());
						editor.processPath(processingPath, app->renderer, app,
										   generateMipmaps);
					} else {
						SDL_Log("No path found");
					}
				}

				if (!outputFile.empty()) {
					fs::path outputText(outputFile);
					std::string filename = outputText.filename().string();
					ImGui::Text("Output: %s", filename.c_str());
				}

				if (texture != nullptr) {
					float width, height;
					SDL_GetTextureSize(texture, &width, &height);

					const float maxWidth =	// Scale down if texture is wider
						150.0f / width;	// than 150 pixels

					const float scaleFactor = std::min(maxWidth, 1.0f);

					const ImVec2 scaledSize(width * scaleFactor,
											height * scaleFactor);

					ImGui::Image(texture, scaledSize);
					if (fallbackToDisk) {
						ImGui::TextWrapped("Loaded from disk");
					}
				}
			}

			bool process(NodeEditor& editor, SDL_Renderer* renderer,
						 Node* previousNode, SDL_Surface*& currentSurface,
						 Application* app) override {
				fallbackToDisk = false;

				if (previousNode == nullptr) {
					SDL_Log("Output node has no upstream node.");
					return false;
				}

				if (texture) {
					SDL_DestroyTexture(texture);
					texture = nullptr;
				}

				if (previousNode->texture) {
					texture = previousNode->texture;
				} else if (currentSurface) {
					texture =
						SDL_CreateTextureFromSurface(renderer, currentSurface);
				} else {
					texture = IMG_LoadTexture(renderer,
											  previousNode->sourceFile.c_str());
					fallbackToDisk = true;
				}

				if (outputFile.empty()) {
					std::string outName;
					if (!inputNode->sourceFile.empty()) {
						// Strip path and extension from sourceFile
						const std::string& src = inputNode->sourceFile;
						fs::path path(src);
						
						std::string fname = path.stem().string();
						fs::path outPath = fs::path(selected_folder)/(fname + "_processed.png");
						outName = outPath.string();

						utils::image_u8 img;

						if (!editor.surfaceToImageU8(currentSurface, img)) {
							SDL_Log(
								"Failed converting output surface to "
								"image_u8.");
							return false;
						}

						if (!utils::save_png(outName.c_str(), img, true)) {
							SDL_Log("Failed saving PNG: %s", outName.c_str());
							return false;
						}

						outputFile = outName;
						SDL_Log("Output node wrote PNG: %s",
								outputFile.c_str());
						return true;
					} else {
						SDL_Log(
							"Output node has no outputFile and no "
							"currentSurface.");
						return false;
					}
				}
				//sourceFile = previousNode->sourceFile;
				outputFile = previousNode->outputFile;

				SDL_Log("Output node received file: %s", outputFile.c_str());
				return true;
			}
		};

		class MipMapNode : public Node {
		   public:
			int mipmapLevels = 1;

			[[nodiscard]] const char* name() const override {
				return "MipMap Node";
			}

			bool process(NodeEditor& editor, SDL_Renderer* renderer,
						 Node* inputNode,
						 SDL_Surface*& currentSurface, Application* app) override {
				return true;
			}
		};

		class EffectNode : public Node {
		   public:
			[[nodiscard]] const char* name() const override {
				return "Effect Node";
			}

			void renderBody(Application* app, NodeEditor& editor) override {
				ImGui::Text("Effect node placeholder");
			}

			bool process(NodeEditor& editor, SDL_Renderer* renderer,
						 Node* inputNode,
						 SDL_Surface*& currentSurface, Application* app) override {
				if (inputNode == nullptr) {
					SDL_Log("Effect node has no input.");
					return false;
				}

				if (!inputNode->process(editor, renderer, inputNode,
										currentSurface, app)) {
					return false;
				}

				// Propagate current surface and texture for preview
				if (currentSurface) {
					if (texture) {
						SDL_DestroyTexture(texture);
						texture = nullptr;
					}
					texture =
						SDL_CreateTextureFromSurface(renderer, currentSurface);
				} else if (inputNode->texture) {
					texture = inputNode->texture;
				}

				sourceFile = inputNode->sourceFile;
				outputFile = inputNode->outputFile;

				return true;
			}
		};

		class CompressionNode : public Node {
		   public:
			bool isFinalCompressionNode = false;
			bool generateMipmapsForExport = false;
			
			enum class CompressionType { BC7, BC5, BC4, BC3, BC2, BC1 };

			CompressionType compressionType = CompressionType::BC7;

			[[nodiscard]] const char* name() const override {
				return "Compression Node";
			}

			void renderBody(Application* app, NodeEditor& editor) override {
				switch (compressionType) {
					case CompressionType::BC7:
						ImGui::Text("Compression: BC7");
						break;
					case CompressionType::BC5:
						ImGui::Text("Compression: BC5");
						break;
					case CompressionType::BC4:
						ImGui::Text("Compression: BC4");
						break;
					case CompressionType::BC3:
						ImGui::Text("Compression: BC3");
						break;
					case CompressionType::BC2:
						ImGui::Text("Compression: BC2");
						break;
					case CompressionType::BC1:
						ImGui::Text("Compression: BC1");
						break;
				}
			}

			bool process(NodeEditor& editor, SDL_Renderer* renderer,
						 Node* inputNode,
						 SDL_Surface*& currentSurface, Application* app) override {
				if (inputNode == nullptr) {
					SDL_Log("Compression node has no input.");
					return false;
				}

				// If we have an in-memory surface from upstream, prefer that.
				SDL_Surface* surfaceToCompress = currentSurface;
				if (!surfaceToCompress && !inputNode->sourceFile.empty()) {
					// Fall back to loading from disk if surface is not
					// available.
					surfaceToCompress = IMG_Load(inputNode->sourceFile.c_str());
					if (!surfaceToCompress) {
						SDL_Log("Failed to load surface from %s: %s",
								inputNode->sourceFile.c_str(), SDL_GetError());
						return false;
					}
				}

				// Choose an output filename. Prefer a descriptive name using
				// the
				// source file base name and compression type so it's easy to
				// find.
				std::string typeName;
				DXGI_FORMAT selectedDxgiFormat = DXGI_FORMAT_UNKNOWN;
				switch (compressionType) {
					case CompressionType::BC7:
						typeName = "BC7";
						selectedDxgiFormat = DXGI_FORMAT_BC7_UNORM;
						break;
					case CompressionType::BC5:
						typeName = "BC5";
						selectedDxgiFormat = DXGI_FORMAT_BC5_UNORM;
						break;
					case CompressionType::BC4:
						typeName = "BC4";
						selectedDxgiFormat = DXGI_FORMAT_BC4_UNORM;
						break;
					case CompressionType::BC3:
						typeName = "BC3";
						selectedDxgiFormat = DXGI_FORMAT_BC3_UNORM;
						break;
					case CompressionType::BC2:
						typeName = "BC2";
						selectedDxgiFormat = DXGI_FORMAT_BC2_UNORM;
						break;
					case CompressionType::BC1:
						typeName = "BC1";
						selectedDxgiFormat = DXGI_FORMAT_BC1_UNORM;
						break;
					default:
						typeName = "BC";
						break;
				}

				std::string outName;
				if (!inputNode->sourceFile.empty()) {
					// Strip path and extension from sourceFile
					const std::string& src = inputNode->sourceFile;
					fs::path path(src);

					std::string fname = path.stem().string();
					fs::path outPath = fs::path(selected_folder) /
									   (fname + "_processed.png");
					outName = outPath.string();
				}

				// Compress to blocks and store on this node.
				std::vector<char> blocks;
				int w = 0, h = 0, bytes_per_block = 0, num_blocks = 0,
					format = 0;
				if (editor.compressSurfaceToBlocks(
						surfaceToCompress, blocks, w, h, bytes_per_block,
						num_blocks, format, compressionType)) {
					this->compressedBlocks = std::move(blocks);
					this->compressedWidth = w;
					this->compressedHeight = h;
					this->compressedBytesPerBlock = bytes_per_block;
					this->compressedNumBlocks = num_blocks;
					this->compressedFormat = format;
					
				} else {
					SDL_Log("Failed to compress surface to blocks.");
					return false;
				}

				
				SDL_Surface* decoded = nullptr;
				if (!this->compressedBlocks.empty()) {
					decoded = editor.decodeBlocksToSurface(
						this->compressedBlocks.data(),
						this->compressedBytesPerBlock,
						this->compressedNumBlocks, this->compressedWidth,
						this->compressedHeight, this->compressedFormat);
				}
				if (decoded) {
					if (texture) {
						SDL_DestroyTexture(texture);
						texture = nullptr;
					}

					texture = SDL_CreateTextureFromSurface(renderer, decoded);

					
				if (isFinalCompressionNode) {
						utils::image_u8 img;

						if (!editor.surfaceToImageU8(surfaceToCompress, img)) {
							SDL_Log("Failed converting surface to image_u8");
							return false;
						}

						rdo_bc::rdo_bc_params params;

						params.m_generate_mipmaps = generateMipmapsForExport;

						params.m_dxgi_format = selectedDxgiFormat;

						rdo_bc::rdo_bc_encoder encoder;

						if (!encoder.init(img, params)) return false;
						if (!encoder.encode()) return false;

						if (utils::save_dds(
								outName.c_str(), encoder.get_orig_width(),
								encoder.get_orig_height(),
								encoder.get_mip_levels(), encoder.get_blocks(),
								encoder.get_pixel_format_bpp(),
								encoder.get_pixel_format(), false, true)) {
							outputFile = outName;
							SDL_Log("Compression node wrote DDS: %s",
									outputFile.c_str());
							return true;
						} else {
							SDL_Log("Compression node failed to write DDS: %s",
									outName.c_str());
							outputFile = "";
							return false;
						}
					}

					SDL_Surface* nextSurface = SDL_DuplicateSurface(decoded);
					if (!nextSurface) {
						SDL_Log("SDL_DuplicateSurface failed: %s",
								SDL_GetError());
						SDL_DestroySurface(decoded);
						return false;
					}

					if (currentSurface) {
						SDL_DestroySurface(currentSurface);
					}

					currentSurface = nextSurface;

					//sourceFile = inputNode->sourceFile;
					outputFile = outName;

					SDL_DestroySurface(decoded);
				}
				return true;
			}
		};

		NodeImGui::NodeId getUniqueId() {
			return NodeImGui::NodeId(uniqueId++);
		}

		NodeImGui::PinId getUniquePinId() {
			return NodeImGui::PinId(uniqueId++);
		}

		NodeEditor() {
			// Initialize rgbcx decoder/encoder tables for BC1-5
			rgbcx::init();

			NodeImGui::Config config;
			config.SettingsFile = "NodeEditor.json";
			nodeContext = NodeImGui::CreateEditor(&config);
		}

		std::vector<std::unique_ptr<Node>> nodes;
		std::vector<Link> links;
		std::vector<GraphEdge> graphEdges;

		void addNode(std::unique_ptr<Node> node) {
			nodes.push_back(std::move(node));
		}

		[[nodiscard]] Node* findNodeById(const NodeImGui::NodeId id) const {
			for (const auto& node : nodes) {
				if (node->id == id) {
					return node.get();
				}
			}
			return nullptr;
		}

		[[nodiscard]] Node* findNodeOwningPin(
			const NodeImGui::PinId pin) const {
			for (const auto& node : nodes) {
				if (node->inputs.contains(pin) || node->outputs.contains(pin)) {
					return node.get();
				}
			}
			return nullptr;
		}

		Node* findUpstreamNode(Node& targetNode) const {
			if (targetNode.inputs.empty()) {
				SDL_Log("findUpstreamNode: target node %zu has no inputs",
						targetNode.id.Get());
				return nullptr;
			}

			SDL_Log("findUpstreamNode: target node %zu has %zu input pins",
					targetNode.id.Get(), targetNode.inputs.size());

			// Search all input pins for an incoming link.
			for (const auto& targetInputPin : targetNode.inputs) {
				SDL_Log("findUpstreamNode: checking inputPin %lu for node %lu",
						targetInputPin.Get(), targetNode.id.Get());
				for (const auto& link : links) {
					SDL_Log(" findUpstreamNode: link id=%lu start=%lu end=%lu",
							link.id.Get(), link.startPin.Get(),
							link.endPin.Get());
					if (link.endPin == targetInputPin) {
						Node* owner = findNodeOwningPin(link.startPin);
						if (owner) {
							SDL_Log(
								"findUpstreamNode: found upstream node %d via "
								"link %d",
								(int)owner->id.Get(), (int)link.id.Get());
						} else {
							SDL_Log(
								"findUpstreamNode: found link %d but owner "
								"not found for startPin %d",
								(int)link.id.Get(), (int)link.startPin.Get());
						}
						return owner;
					}
				}
			}

			SDL_Log("findUpstreamNode: no upstream found for node %lu",
					targetNode.id.Get());
			return nullptr;
		}

		void addLink(const Link& link) { links.push_back(link); }

		void removeNode(NodeImGui::NodeId nodeToDelete) {
			const auto id = std::find_if(nodes.begin(), nodes.end(),
										 [nodeToDelete](const auto& node) {
											 return node->id == nodeToDelete;
										 });

			if (id != nodes.end()) {
				graphEdges.erase(
					std::remove_if(graphEdges.begin(), graphEdges.end(),
								   [id](const auto& edge) {
									   return edge.source == id->get() ||
											  edge.destination == id->get();
								   }),
					graphEdges.end());
				nodes.erase(id);
			}
		}

		void removeLink(NodeImGui::LinkId linkToDelete) {
			const auto id = std::find_if(
				links.begin(), links.end(),
				[linkToDelete](auto& link) { return link.id == linkToDelete; });
			if (id != links.end()) {
				graphEdges.erase(
					std::remove_if(
						graphEdges.begin(), graphEdges.end(),
						[id](const auto& edge) {
							return (edge.source->inputs.contains(
										id->startPin) ||
									edge.source->outputs.contains(
										id->startPin)) &&
								   (edge.destination->inputs.contains(
										id->endPin) ||
									edge.destination->outputs.contains(
										id->endPin));
						}),
					graphEdges.end());
				links.erase(id);
			}
		}

		// Compress an in-memory SDL_Surface to a DDS file using the specified
		// compression type. Useful when the pipeline keeps data in-memory
		// instead of writing intermediate files.

		// Compress an in-memory SDL_Surface into raw compressed blocks and
		// metadata. Returns true on success and fills `outBlocks` and metadata.
		bool compressSurfaceToBlocks(
			SDL_Surface* inputSurface, std::vector<char>& outBlocks,
			int& outWidth, int& outHeight, int& outBytesPerBlock,
			int& outNumBlocks, int& outFormat,
			CompressionNode::CompressionType compressionType) {
			SDL_Log("compressSurfaceToBlocks: starting compression for type %d",
					static_cast<int>(compressionType));
			if (inputSurface == nullptr) {
				SDL_Log("compressSurfaceToBlocks: inputSurface is null");
				return false;
			}

			SDL_Surface* surface =
				SDL_ConvertSurface(inputSurface, SDL_PIXELFORMAT_RGBA32);
			if (surface == nullptr) {
				SDL_Log("SDL_ConvertSurface failed: %s", SDL_GetError());
				return false;
			}

			// If BC7, use existing encoder (libbc7enc). For BC1/3/4/5 use rgbcx
			// block encoder.
			if (compressionType == CompressionNode::CompressionType::BC7) {
				encode_output output = {};
				rdo_bc::rdo_bc_params params = {};

				params.m_dxgi_format = DXGI_FORMAT_BC7_UNORM;
				bc7enc_compress_image_from_memory(
					surface->w, surface->h, surface->pixels, params, &output);

				if (output.blocks && output.num_blocks > 0 &&
					output.bytes_per_block > 0) {
					const size_t totalBytes =
						static_cast<size_t>(output.num_blocks) *
						static_cast<size_t>(output.bytes_per_block);
					outBlocks.assign(output.blocks, output.blocks + totalBytes);
					outWidth = output.width;
					outHeight = output.height;
					outBytesPerBlock = output.bytes_per_block;
					outNumBlocks = output.num_blocks;
					outFormat = static_cast<int>(output.format);
				} else {
					SDL_Log(
						"compressSurfaceToBlocks: encoder produced no blocks");
					bc7enc_free_encode_output(&output);
					SDL_DestroySurface(surface);
					return false;
				}

				bc7enc_free_encode_output(&output);
				SDL_DestroySurface(surface);
				return true;
			} else {
				// Use rgbcx encoder for BC1/3/4/5
				const int width = surface->w;
				const int height = surface->h;
				const int blocks_x = (width + 3) / 4;
				const int blocks_y = (height + 3) / 4;
				const int num_blocks = blocks_x * blocks_y;
				int bytes_per_block = 0;
				int dxgi_format = 0;

				switch (compressionType) {
					case CompressionNode::CompressionType::BC1:
						bytes_per_block = 8;
						dxgi_format = DXGI_FORMAT_BC1_UNORM;
						break;
					case CompressionNode::CompressionType::BC3:
						bytes_per_block = 16;
						dxgi_format = DXGI_FORMAT_BC3_UNORM;
						break;
					case CompressionNode::CompressionType::BC4:
						bytes_per_block = 8;
						dxgi_format = DXGI_FORMAT_BC4_UNORM;
						break;
					case CompressionNode::CompressionType::BC5:
						bytes_per_block = 16;
						dxgi_format = DXGI_FORMAT_BC5_UNORM;
						break;

					default:
						SDL_DestroySurface(surface);
						return false;
				}

				outBlocks.resize(static_cast<size_t>(num_blocks) *
								 bytes_per_block);

				// temporary block pixel buffer (RGBA)
				uint8_t block_pixels[16 * 4];

				for (int by = 0; by < blocks_y; ++by) {
					for (int bx = 0; bx < blocks_x; ++bx) {
						// fill block_pixels from surface, pad with zeros for
						// out-of-range pixels
						for (int py = 0; py < 4; ++py) {
							for (int px = 0; px < 4; ++px) {
								const int x = bx * 4 + px;
								const int y = by * 4 + py;
								const int idx = (py * 4 + px) * 4;
								if (x < width && y < height) {
									const uint8_t* src =
										reinterpret_cast<uint8_t*>(
											surface->pixels) +
										(y * surface->pitch) + (x * 4);
									block_pixels[idx + 0] = src[0];
									block_pixels[idx + 1] = src[1];
									block_pixels[idx + 2] = src[2];
									block_pixels[idx + 3] = src[3];
								} else {
									block_pixels[idx + 0] = 0;
									block_pixels[idx + 1] = 0;
									block_pixels[idx + 2] = 0;
									block_pixels[idx + 3] = 255;
								}
							}
						}

						void* dst = outBlocks.data() +
									(static_cast<size_t>(by * blocks_x + bx) *
									 bytes_per_block);
						switch (compressionType) {
							// Use low quality / fast mode for interactive
							// debugging.
							case CompressionNode::CompressionType::BC1:
								rgbcx::encode_bc1(0, dst, block_pixels, false,
												  false);
								break;
							case CompressionNode::CompressionType::BC3:
								rgbcx::encode_bc3(0, dst, block_pixels);
								break;
							case CompressionNode::CompressionType::BC4:
								rgbcx::encode_bc4(dst, block_pixels, 4);
								break;
							case CompressionNode::CompressionType::BC5:
								rgbcx::encode_bc5(dst, block_pixels, 0, 1, 4);
								break;
							case CompressionNode::CompressionType::BC2:
								rgbcx::encode_bc3(
									10, dst,
									block_pixels);	// approx BC2 with BC3
								break;
							default:
								break;
						}
					}
				}

				outWidth = width;
				outHeight = height;
				outBytesPerBlock = bytes_per_block;
				outNumBlocks = num_blocks;
				outFormat = dxgi_format;

				SDL_DestroySurface(surface);
				return true;
			}
		}

		

		void cleanup() const { NodeImGui::DestroyEditor(nodeContext); }

		void render(Application* a) {
			NodeImGui::SetCurrentEditor(nodeContext);
			NodeImGui::Begin("Node Editor");

			for (const auto& node : nodes) {
				NodeImGui::BeginNode(node->id);

				// Push a unique ImGui ID per node to avoid "SameID" collisions
				// for widgets (buttons, etc.) inside different nodes.
				ImGui::PushID(static_cast<int>(node->id.Get()));

				ImGui::Text("%s - ID: %lu", node->name(), node->id.Get());

				for (const auto& inputPin : node->inputs) {
					NodeImGui::BeginPin(inputPin, NodeImGui::PinKind::Input);
					ImGui::Text("-> %s", node->getInputLabel(inputPin));
					NodeImGui::EndPin();
				}

				node->renderBody(a, *this);
				for (const auto& outputPin : node->outputs) {
					NodeImGui::BeginPin(outputPin, NodeImGui::PinKind::Output);
					ImGui::Text("-> %s", node->getOutputLabel(outputPin));
					NodeImGui::EndPin();
					
				}

				ImGui::PopID();

				NodeImGui::EndNode();
			}

			for (const auto& link : links) {
				NodeImGui::Link(link.id, link.startPin, link.endPin);
				Node* sourceNode = findNodeOwningPin(link.startPin);
				Node* destNode = findNodeOwningPin(link.endPin);
				GraphEdge edge{sourceNode, destNode};
			}

			if (NodeImGui::BeginCreate()) {
				NodeImGui::PinId firstPin;
				NodeImGui::PinId secondPin;

				if (NodeImGui::QueryNewLink(&firstPin, &secondPin)) {
					if (firstPin && secondPin && NodeImGui::AcceptNewItem()) {
						Node* firstNode = findNodeOwningPin(firstPin);
						Node* secondNode = findNodeOwningPin(secondPin);

						if (firstNode != nullptr && secondNode != nullptr &&
							firstNode->id != secondNode->id) {
							const bool firstIsInput =
								firstNode->inputs.contains(firstPin);
							const bool firstIsOutput =
								firstNode->outputs.contains(firstPin);
							const bool secondIsInput =
								secondNode->inputs.contains(secondPin);
							const bool secondIsOutput =
								secondNode->outputs.contains(secondPin);

							Link link;
							link.id = uniqueId++;

							if (firstIsOutput && secondIsInput) {
								link.startPin = firstPin;
								link.endPin = secondPin;

								GraphEdge edge;
								edge.source = firstNode;
								edge.destination = secondNode;
								graphEdges.push_back(edge);
								links.push_back(link);

								SDL_Log("Created link id=%d start=%d end=%d",
										(int)link.id.Get(), (int)link.startPin.Get(),
										(int)link.endPin.Get());
							} else if (firstIsInput && secondIsOutput) {
								link.startPin = secondPin;
								link.endPin = firstPin;

								GraphEdge edge;
								edge.source = secondNode;
								edge.destination = firstNode;
								graphEdges.push_back(edge);
								links.push_back(link);
								SDL_Log("Created link id=%d start=%d end=%d",
										(int)link.id.Get(), (int)link.startPin.Get(),
										(int)link.endPin.Get());
							}
						}
					}
				}
				NodeImGui::EndCreate();
			}

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

		void reset() {
			nodes.clear();
			links.clear();
			graphEdges.clear();
			uniqueId = 1;
		}

		// Will return the node-path as a vector of Node* from the output node
		// to the input node, or an empty vector if no path exists
		[[nodiscard]] std::vector<Node*> findPathBetweenInputAndOutput(
			const NodeImGui::NodeId outputNodeId) const {
			std::stack<std::pair<Node*, std::vector<Node*>>>
				st;	 // branch stack and saved branch paths
			std::unordered_set<Node*> visited;
			std::vector<Node*> path = {};
			Node* currentNode = findNodeById(outputNodeId);

			st.push(std::make_pair(currentNode, path));

			while (!st.empty()) {
				currentNode = st.top().first;
				path = st.top().second;
				st.pop();

				if (visited.find(currentNode) != visited.end()) {
					continue;
				}
				visited.insert(currentNode);
				path.push_back(currentNode);

				if (currentNode->isInputNode) {
					std::reverse(path.begin(), path.end());
					return path;
				}

				for (const auto& edge : graphEdges) {
					if (edge.destination == currentNode) {
						if (visited.find(edge.source) == visited.end()) {
							st.push(std::make_pair(edge.source, path));
						}
					}
				}
			}
			return {};
		}

		// Process an ordered path from input (path[0]) to output (path.back()).
		// For each node in the path (skipping the input node) set its
		// `sourceFile` to the current file, call its `process` method and
		// advance the current file to the node's `outputFile` if it was set.
		bool processPath(const std::vector<Node*>& path,
						 SDL_Renderer* renderer, Application* app, bool generateMipmaps) {
			if (path.empty()) {
				SDL_Log("processPath: empty path");
				return false;
			}
			// First element should be the input node and must contain either a
			// surface or a source file.
			const Node* inputNode = path.front();
			if (!inputNode) {
				SDL_Log("processPath: input node null");
				return false;
			}

			// Put mipmap finding somewhere here, maybe as a separate pass
			// before processing the path, to set a flag on each node whether it
			// needs to generate mipmaps or not.

			// Initialize currentSurface from the input node if available,
			// otherwise try to load from disk.
			SDL_Surface* currentSurface = nullptr;

			if (inputNode->surface) {
				currentSurface = SDL_DuplicateSurface(inputNode->surface);
			} else if (!inputNode->sourceFile.empty()) {
				currentSurface = IMG_Load(inputNode->sourceFile.c_str());
			}

			CompressionNode* lastCompressionNode = nullptr;

			if (generateMipmaps) {
			SDL_Log("processPath: mipmap generation enabled");
			}
				for (Node* node : path) {
					if (auto* compression = dynamic_cast<CompressionNode*>(node)) {
						lastCompressionNode = compression;
						
					}
				}
			

			// Iterate nodes after the input node and feed the currentSurface
			// through
			for (size_t i = 1; i < path.size(); ++i) {
				Node* node = path[i];
				if (node == nullptr) {
					SDL_Log("processPath: encountered null node in path");
					if (currentSurface) SDL_DestroySurface(currentSurface);
					return false;
				}
				SDL_Log("processPath: running %s (ID: %d)", node->name(),
						(int)node->id.Get());
				if (auto* compression = dynamic_cast<CompressionNode*>(node)) {
					compression->isFinalCompressionNode =
						compression == lastCompressionNode;

					compression->generateMipmapsForExport =
						generateMipmaps && compression == lastCompressionNode;
				}
				if (!node->process(*this, renderer, path[i - 1],
								   currentSurface, app)) {
					SDL_Log(
						"processPath: node processing failed for %s (ID: %d)",
						node->name(), (int)node->id.Get());
					if (currentSurface) SDL_DestroySurface(currentSurface);
					return false;
				}
			}

			if (currentSurface) {
				SDL_DestroySurface(currentSurface);
				currentSurface = nullptr;
			}

			SDL_Log("processPath: finished");
			return true;
		}


		static bool surfaceToImageU8(SDL_Surface* src, utils::image_u8& dst) {
			if (!src) return false;

			SDL_Surface* rgba = SDL_ConvertSurface(src, SDL_PIXELFORMAT_RGBA32);

			if (!rgba) return false;

			dst.init(rgba->w, rgba->h);

			uint8_t* pixels = static_cast<uint8_t*>(rgba->pixels);

			for (int y = 0; y < rgba->h; ++y) {
				for (int x = 0; x < rgba->w; ++x) {
					uint8_t* p = pixels + y * rgba->pitch + x * 4;

					utils::color_quad_u8 c;

					c.r = p[0];
					c.g = p[1];
					c.b = p[2];
					c.a = p[3];

					dst(x, y) = c;
				}
			}

			SDL_DestroySurface(rgba);

			return true;
		}

		// Decode raw compressed blocks (from any encoder) into an SDL_Surface
		// for preview.
		SDL_Surface* decodeBlocksToSurface(const void* blocks,
										   const int bytes_per_block,
										   const int num_blocks,
										   const int width, const int height,
										   const int format) {
			if (blocks == nullptr || num_blocks <= 0 || width <= 0 ||
				height <= 0)
				return nullptr;

			constexpr int bytes_per_pixel = 4;
			std::vector<uint8_t> pixels(static_cast<size_t>(width) *
										static_cast<size_t>(height) *
										bytes_per_pixel);

			const int blocks_x = (width + 3) / 4;
			const int blocks_y = (height + 3) / 4;
			int bc1_failures = 0;

			for (int by = 0; by < blocks_y; ++by) {
				for (int bx = 0; bx < blocks_x; ++bx) {
					const int block_index = by * blocks_x + bx;
					if (block_index >= num_blocks) break;
					const uint8_t* block_ptr =
						reinterpret_cast<const uint8_t*>(blocks) +
						static_cast<size_t>(block_index) *
							static_cast<size_t>(bytes_per_block);
					uint8_t block_pixels[16 * 4];
					memset(block_pixels, 0, sizeof(block_pixels));

					if (format == DXGI_FORMAT_BC7_UNORM) {
						bc7decomp::color_rgba out_colors[16];
						bc7decomp::unpack_bc7(block_ptr, out_colors);
						for (int i = 0; i < 16; ++i) {
							block_pixels[i * 4 + 0] = out_colors[i].r;
							block_pixels[i * 4 + 1] = out_colors[i].g;
							block_pixels[i * 4 + 2] = out_colors[i].b;
							block_pixels[i * 4 + 3] = out_colors[i].a;
						}
					} else if (format == DXGI_FORMAT_BC1_UNORM) {
						rgbcx::unpack_bc1(block_ptr, block_pixels, true,
										  rgbcx::bc1_approx_mode::cBC1Ideal);

					} else if (format == DXGI_FORMAT_BC3_UNORM) {
						rgbcx::unpack_bc3(block_ptr, block_pixels);
					} else if (format == DXGI_FORMAT_BC4_UNORM) {
						uint8_t single[16];
						rgbcx::unpack_bc4(block_ptr, single, 1);
						for (int i = 0; i < 16; ++i) {
							block_pixels[i * 4 + 0] = single[i];
							block_pixels[i * 4 + 1] = single[i];
							block_pixels[i * 4 + 2] = single[i];
							block_pixels[i * 4 + 3] = 255;
						}
					} else if (format == DXGI_FORMAT_BC5_UNORM) {
						// rgbcx::unpack_bc5 has a void return type; it fills
						// `block_pixels` directly.
						rgbcx::unpack_bc5(block_ptr, block_pixels, 0, 1, 4);
						for (int i = 0; i < 16; ++i) {
							uint8_t r = block_pixels[i * 4 + 0];
							uint8_t g = block_pixels[i * 4 + 1];
							block_pixels[i * 4 + 2] = 0;
							block_pixels[i * 4 + 3] = 255;
						}
					} else {
						SDL_Log(
							"decodeBlocksToSurface: unsupported format %d for "
							"block %d",
							format, block_index);
						continue;
					}

					for (int py = 0; py < 4; ++py) {
						for (int px = 0; px < 4; ++px) {
							const int x = bx * 4 + px;
							const int y = by * 4 + py;
							if (x >= width || y >= height) continue;
							const int dst = (y * width + x) * bytes_per_pixel;
							const int src = py * 4 + px;
							pixels[dst + 0] = block_pixels[src * 4 + 0];
							pixels[dst + 1] = block_pixels[src * 4 + 1];
							pixels[dst + 2] = block_pixels[src * 4 + 2];
							pixels[dst + 3] = block_pixels[src * 4 + 3];
						}
					}
				}
			}

			SDL_Surface* surf =
				SDL_CreateSurface(width, height, SDL_PIXELFORMAT_RGBA32);
			if (!surf) {
				SDL_Log(
					"decodeBlocksToSurface: SDL_CreateRGBSurfaceWithFormat "
					"failed: %s",
					SDL_GetError());
				return nullptr;
			}
			if (SDL_MUSTLOCK(surf)) SDL_LockSurface(surf);
			memcpy(surf->pixels, pixels.data(), pixels.size());
			if (SDL_MUSTLOCK(surf)) SDL_UnlockSurface(surf);

			return surf;
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
		auto node = std::make_unique<NodeEditor::Node>();

		node->id = nodeEditor.getUniqueId();

		for (int i = 0; i < 2; ++i) {
			node->inputs.push_back(nodeEditor.getUniquePinId());
		}

		for (int i = 0; i < 2; ++i) {
			node->outputs.push_back(nodeEditor.getUniquePinId());
		}

		const ImVec2 center = ImGui::GetMainViewport()->GetCenter();
		NodeImGui::SetNodePosition(node->id, ImVec2(center.x, center.y));

		nodeEditor.addNode(std::move(node));
	}

	void CreateInputNode() {
		auto node = std::make_unique<NodeEditor::InputNode>();

		node->id = nodeEditor.getUniqueId();
		node->outputs.push_back(nodeEditor.getUniquePinId());
		node->isInputNode = true;

		const ImVec2 center = ImGui::GetMainViewport()->GetCenter();
		NodeImGui::SetNodePosition(node->id, ImVec2(center.x, center.y));

		nodeEditor.addNode(std::move(node));
	}

	void CreateOutputNode() {
		auto node = std::make_unique<NodeEditor::OutputNode>();

		node->id = nodeEditor.getUniqueId();
		node->inputs.push_back(nodeEditor.getUniquePinId());
		node->isOutputNode = true;

		const ImVec2 center = ImGui::GetMainViewport()->GetCenter();
		NodeImGui::SetNodePosition(node->id, ImVec2(center.x, center.y));

		nodeEditor.addNode(std::move(node));
	}

	void CreateCompressionNode(
		const NodeEditor::CompressionNode::CompressionType compressionType) {
		auto node = std::make_unique<NodeEditor::CompressionNode>();
		node->compressionType = compressionType;
		node->id = nodeEditor.getUniqueId();
		node->inputs.push_back(nodeEditor.getUniquePinId());
		node->outputs.push_back(nodeEditor.getUniquePinId());

		const ImVec2 center = ImGui::GetMainViewport()->GetCenter();
		NodeImGui::SetNodePosition(node->id, ImVec2(center.x, center.y));

		nodeEditor.addNode(std::move(node));
	}

	void CreateRGBASplitNode() {
    auto node = std::make_unique<NodeEditor::RGBASplitNode>();

    node->id = nodeEditor.getUniqueId();

    node->inputs.push_back(nodeEditor.getUniquePinId());

    node->redOutput = nodeEditor.getUniquePinId();
    node->greenOutput = nodeEditor.getUniquePinId();
    node->blueOutput = nodeEditor.getUniquePinId();
    node->alphaOutput = nodeEditor.getUniquePinId();

    node->outputs.push_back(node->redOutput);
    node->outputs.push_back(node->greenOutput);
    node->outputs.push_back(node->blueOutput);
    node->outputs.push_back(node->alphaOutput);

    node->outputPinKinds[node->redOutput.Get()] =
		NodeEditor::Node::PinDataKind::Red;

	node->outputPinKinds[node->greenOutput.Get()] =
		NodeEditor::Node::PinDataKind::Green;

	node->outputPinKinds[node->blueOutput.Get()] =
		NodeEditor::Node::PinDataKind::Blue;

	node->outputPinKinds[node->alphaOutput.Get()] =
		NodeEditor::Node::PinDataKind::Alpha;

	const ImVec2 center = ImGui::GetMainViewport()->GetCenter();
	NodeImGui::SetNodePosition(node->id, ImVec2(center.x, center.y));

    nodeEditor.addNode(std::move(node));
}

	void CreateRGBAToNewSurfaceNode() {
	auto node = std::make_unique<NodeEditor::RGBAToNewSurfaceNode>();

	node->id = nodeEditor.getUniqueId();

	node->redInput = nodeEditor.getUniquePinId();
	node->greenInput = nodeEditor.getUniquePinId();
	node->blueInput = nodeEditor.getUniquePinId();
	node->alphaInput = nodeEditor.getUniquePinId();

	node->inputs.push_back(node->redInput);
	node->inputs.push_back(node->greenInput);
	node->inputs.push_back(node->blueInput);
	node->inputs.push_back(node->alphaInput);

	node->imageOutput = nodeEditor.getUniquePinId();
	node->outputs.push_back(node->imageOutput);

	const ImVec2 center = ImGui::GetMainViewport()->GetCenter();
	NodeImGui::SetNodePosition(node->id, ImVec2(center.x, center.y));

	nodeEditor.addNode(std::move(node));
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
				"operation cannot be undone.");
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

		if (ImGui::CollapsingHeader("RGBA splitting")) {
			ImGui::Dummy(ImVec2(0.0f, 10.0f));
			if (ImGui::Button("Create RGBA-split Node")) {
				CreateRGBASplitNode();
			}
			ImGui::SameLine();
			HelpMarker(
				"This node will be used to split the RGBA channels of the "
				"input images. You can "
				"process each channel separately, allowing for more advanced "
				"image manipulation.");

			if (ImGui::Button("Create RGBA-surface Node")) {
				CreateRGBAToNewSurfaceNode();
			}
			ImGui::SameLine();
			HelpMarker(
				"This node will be used to create an RGBA surface from the "
				"input images. You can "
				"process each channel separately, allowing for more advanced "
				"image manipulation.");
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
				"the filtering method to use.");
			ImGui::SeparatorText("Options");
		}
		if (ImGui::CollapsingHeader("Effects")) {
			ImGui::Dummy(ImVec2(0.0f, 10.0f));
			ImGui::BeginChild("ChildR", ImVec2(0, 160));
			if (ImGui::BeginTable("Effects' table", 1)) {
				ImGui::TableNextColumn();
				// Placeholder-buttons
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

				if (ImGui::Button("Create BC7 Compression Node")) {
					CreateCompressionNode(
						NodeEditor::CompressionNode::CompressionType::BC7);
				}
				if (ImGui::Button("BC6S")) {
				}
				if (ImGui::Button("ASTC")) {
				}

				if (ImGui::Button("Create BC5 Compression Node")) {
					CreateCompressionNode(
						NodeEditor::CompressionNode::CompressionType::BC5);
				}

				if (ImGui::Button("Create BC4 Compression Node")) {
					CreateCompressionNode(
						NodeEditor::CompressionNode::CompressionType::BC4);
				}

				if (ImGui::Button("Create BC3 Compression Node")) {
					CreateCompressionNode(
						NodeEditor::CompressionNode::CompressionType::BC3);
				}

				if (ImGui::Button("Create BC2 Compression Node")) {
					CreateCompressionNode(
						NodeEditor::CompressionNode::CompressionType::BC2);
				}

				if (ImGui::Button("Create BC1 Compression Node")) {
					CreateCompressionNode(
						NodeEditor::CompressionNode::CompressionType::BC1);
				}

				if (ImGui::Button("8")) {
				}

				if (ImGui::Button("USTC")) {
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
				"Converts the image to a tangent-space normal map: exports"
				" the normal vector (x y z) as the color (0.5*x + 0.5, "
				"0.5*y + 0.5 , 0.5*z + 0.5)");
			if (ImGui::Button("Normal Map: Object Space")) {
			}
			ImGui::SameLine();
			HelpMarker(
				"Converts the image to an object-space normal map: exports "
				"the normal vector (x y z) as the color (saknas text) and "
				"can apply cube map coordinate space conversion if the "
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
	void ShowMainMenuBar() const {
		if (ImGui::BeginMainMenuBar()) {
			if (ImGui::BeginMenu("File")) {
				if (ImGui::MenuItem("Select folder...")) {
					SDL_ShowOpenFolderDialog(folder_callback, nullptr, window, nullptr, false);
				}
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
				if (ImGui::MenuItem("GitHub")) {
					SDL_OpenURL(
						"https://github.com/MuhammedReza07/dd1367-project");
				}
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
			// ImGui::ShowDemoWindow();

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
