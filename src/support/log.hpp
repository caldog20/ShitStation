#pragma once
#include "fmt/color.h"
#include "fmt/format.h"

namespace Log {

static constexpr bool debugBuild() {
#ifdef NDEBUG
    return false;
#endif
    return true;
}

//	template <typename... Args>
//	static void warn(const char* fmt, Args&... args) {
//		fmt::print(fg(fmt::color::red), fmt, args...);
//	}

template <typename... Args>
static void warn(const char* fmt, const Args&... args) {
    fmt::print(fg(fmt::color::red), fmt, args...);
}

template <typename... Args>
static void debug(const char* fmt, const Args&... args) {
    if constexpr (debugBuild()) {
        fmt::print(fg(fmt::color::green), fmt, args...);
    }
}

template <typename... Args>
static void info(const char* fmt, Args&... args) {
    fmt::print(fg(fmt::color::white), fmt, std::forward<Args>(args)...);
}

template <typename... Args>
static void info(const char* fmt, Args&&... args) {
    fmt::print(fg(fmt::color::white), fmt, std::forward<Args>(args)...);
}

template <typename... Args>
static void log(fmt::format_string<Args...> fmt, Args&... args) {
    fmt::print(fmt, std::forward<Args>(args)...);
}

}  // namespace Log
