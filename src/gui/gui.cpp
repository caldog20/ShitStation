#include "gui.hpp"

#include "support/utils.hpp"

namespace GUI {

		GUI::GUI() {
				if (SDL_Init(SDL_INIT_VIDEO) != 0) {
						Helpers::panic("Error initializing SDL: {}", SDL_GetError());
				}

				SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);
				SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 4);
				SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 1);

				SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);
				SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE, 24);
				SDL_GL_SetAttribute(SDL_GL_STENCIL_SIZE, 8);

				auto window_flags = (SDL_WindowFlags)(SDL_WINDOW_OPENGL | SDL_WINDOW_RESIZABLE | SDL_WINDOW_ALLOW_HIGHDPI);

				window = SDL_CreateWindow(windowTitle, SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, width, height, window_flags);
				if (window == nullptr) {
						Helpers::panic("Error creating SDL Window: {}", SDL_GetError());
				}

				glContext = SDL_GL_CreateContext(window);
				if (glContext == 0) {
						Helpers::panic("Error creating OpenGL Context: {}", SDL_GetError());
				}

				SDL_GL_MakeCurrent(window, glContext);
				SDL_GL_SetSwapInterval(1);  // VSync on by default

				if (!gladLoadGL((GLADloadfunc)SDL_GL_GetProcAddress)) {
						Helpers::panic("Error initializing glad GL Loader: {}", SDL_GetError());
				}

				IMGUI_CHECKVERSION();
				ImGui::CreateContext();
				ImGui::StyleColorsDark();

				ImGuiIO& io = ImGui::GetIO();
				io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
				io.ConfigFlags |= ImGuiConfigFlags_NavEnableGamepad;
				io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;
				io.ConfigFlags |= ImGuiConfigFlags_ViewportsEnable;

				ImGui_ImplSDL2_InitForOpenGL(window, glContext);
				ImGui_ImplOpenGL3_Init(glslVersion);
		}

		GUI::~GUI() {
				ImGui_ImplOpenGL3_Shutdown();
				ImGui_ImplSDL2_Shutdown();
				ImGui::DestroyContext();

				SDL_GL_DeleteContext(glContext);
				SDL_DestroyWindow(window);
				SDL_Quit();
		}

		void GUI::init() {}

		void GUI::update() {
				while (SDL_PollEvent(&event)) {
						ImGui_ImplSDL2_ProcessEvent(&event);
						if (event.type == SDL_QUIT) close();
						if (event.type == SDL_WINDOWEVENT && event.window.event == SDL_WINDOWEVENT_CLOSE && event.window.windowID == SDL_GetWindowID(window)) {
								close();
						}
				}

				startFrame();

				if (showDemo) ImGui::ShowDemoWindow();
				drawMenuBar();

				endFrame();
		}

		void GUI::shutdown() {}

		void GUI::startFrame() {
				ImGui_ImplOpenGL3_NewFrame();
				ImGui_ImplSDL2_NewFrame();
				ImGui::NewFrame();
		}

		void GUI::endFrame() {
				ImGui::Render();
				ImGuiIO& io = ImGui::GetIO();
				glViewport(0, 0, (int)io.DisplaySize.x, (int)io.DisplaySize.y);
				clearGUI();
				glClear(GL_COLOR_BUFFER_BIT);
				ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

				if (io.ConfigFlags & ImGuiConfigFlags_ViewportsEnable) {
						SDL_Window* backup_current_window = SDL_GL_GetCurrentWindow();
						SDL_GLContext backup_current_context = SDL_GL_GetCurrentContext();
						ImGui::UpdatePlatformWindows();
						ImGui::RenderPlatformWindowsDefault();
						SDL_GL_MakeCurrent(backup_current_window, backup_current_context);
				}

				SDL_GL_SwapWindow(window);
		}

		void GUI::drawMenuBar() {
				if (ImGui::BeginMainMenuBar()) {
						if (ImGui::BeginMenu("File")) {
								if (ImGui::MenuItem("Quit", nullptr)) {
										close();
								}
								ImGui::EndMenu();
						}

						if (ImGui::BeginMenu("Settings")) {
								if (ImGui::MenuItem("Vsync Enabled", nullptr, &vsync)) {
										setVsync(vsync);
								}
								ImGui::EndMenu();
						}

						if (ImGui::BeginMenu("Help")) {
								if (ImGui::MenuItem("Show ImGui Demo", nullptr, &showDemo)) {
								}

								ImGui::EndMenu();
						}

						ImGui::SameLine();
						auto& io = ImGui::GetIO();
						auto menuStatus = fmt::format("FPS: {:.2f} / {:.3f}ms - VSync: {}", io.Framerate, 1000.0f / io.Framerate, vsync ? "On" : "Off");
						textCentered(menuStatus);

						ImGui::EndMainMenuBar();
				}
		}

		void GUI::clearGUI(float r, float g, float b, float a, bool enableGLDepth, bool clearImGUIframe) {
				glClearColor(r, g, b, a);

				if (clearImGUIframe) {
						ImGui_ImplSDL2_NewFrame();
				}

				if (enableGLDepth) {
						// enable the GL_DEPTH_TEST to be able to see 3D object correctly
						glEnable(GL_DEPTH_TEST);
						// clear both the color buffer bit and the depth buffer bit
						glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
				} else {
						glDisable(GL_DEPTH_TEST);
						// clear just the color buffer bit
						glClear(GL_COLOR_BUFFER_BIT);
				}
		}

		void GUI::setVsync(bool value) {
				SDL_GL_SetSwapInterval(static_cast<int>(value));
				vsync = value;
		}

}  // namespace GUI
