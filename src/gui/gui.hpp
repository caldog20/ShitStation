#pragma once
// clang-format off
#include "glad/gl.h"
// clang-format on
#include <SDL.h>
#include <SDL_opengl.h>

#include <algorithm>

#include "imgui.h"
#include "imgui_impl_opengl3.h"
#include "imgui_impl_sdl2.h"
#include "support/imgui_utils.hpp"

namespace GUI {

		class GUI {
			public:
				GUI();
				~GUI();

				void init();
				void update();

				[[nodiscard]] bool isOpen() const { return m_open; }
				void close() { m_open = false; }
				void clearGUI(float r = 0.07f, float g = 0.13f, float b = 0.2f, float a = 1.0f, bool enableGLDepth = false, bool clearImGUIframe = false);

				void setVsync(bool value);

			private:
				void startFrame();
				void endFrame();
				void shutdown();

				void drawMenuBar();

				SDL_Window* window = nullptr;
				SDL_GLContext glContext;
				SDL_Event event;

				bool showDemo = false;
				bool m_open = true;
				bool vsync = true;

				static constexpr int width = 1280;
				static constexpr int height = 720;
				static constexpr const char* glslVersion = "#version 410";
				static constexpr const char* windowTitle = "ShitStation";

				static void normalizeDimensions(ImVec2& vec, float ratio) {
						float r = vec.y / vec.x;
						if (r > ratio) {
								vec.y = vec.x * ratio;
						} else {
								vec.x = vec.y / ratio;
						}
						vec.x = roundf(vec.x);
						vec.y = roundf(vec.y);
						vec.x = std::max(vec.x, 1.0f);
						vec.y = std::max(vec.y, 1.0f);
				}

				static void textCentered(const std::string& text) {
						auto windowWidth = ImGui::GetWindowSize().x;
						auto textWidth = ImGui::CalcTextSize(text.c_str()).x;

						ImGui::SetCursorPosX((windowWidth - textWidth) * 0.5f);
						ImGui::TextFmt(fmt::runtime(text));
				}
		};

}  // namespace GUI
