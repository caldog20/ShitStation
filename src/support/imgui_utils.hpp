#pragma once
#include "fmt/format.h"
#include "imgui.h"

namespace ImGui {

		template <typename... Args>
		IMGUI_API void TextFmt(fmt::format_string<Args...> s, Args&&... args) {
				std::string str = fmt::format(s, std::forward<Args>(args)...);
				ImGui::TextUnformatted(&*str.begin(), &*str.end());
		}

		// Below wrapper fails to properly evaluated compile time strings
		// template <typename T, typename... Args>
		// IMGUI_API void  TextFmt(T&& fmt, const Args &... args) {
		//    std::string str = fmt::format(std::forward<T>(fmt), args...);
		//    ImGui::TextUnformatted(&*str.begin(), &*str.end());
		//}

#define IM_MENU_START(name) if (ImGui::BeginMenu(name)) {
#define IM_MENU_END   \
		ImGui::EndMenu(); \
		}

}  // namespace ImGui
