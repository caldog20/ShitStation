#pragma once

#include <cstdint>
#include <string>
#include <utility>

#include "fmt/color.h"
#include "fmt/format.h"

using u8 = std::uint8_t;
using u16 = std::uint16_t;
using u32 = std::uint32_t;
using u64 = std::uint64_t;

using s8 = std::int8_t;
using s16 = std::int16_t;
using s32 = std::int32_t;
using s64 = std::int64_t;

using Cycles = u64;

static constexpr bool debugBuild() {
#ifdef NDEBUG
	return false;
#endif
	return true;
}

template <typename T>
struct Range {
	Range(T begin, T size) : start(begin), length(size) { static_assert(std::is_integral<T>::value, "Range should take an unsigned integer type"); }

	inline bool contains(u32 addr) const { return (addr >= start && addr < start + length); }

	inline u32 offset(u32 addr) const { return addr - start; }

	T start = 0;
	T length = 0;
};

constexpr auto operator""_Kb(u64 const x) -> u64 { return 1024UL * x; }
constexpr auto operator""_Mb(u64 const x) -> u64 { return 1024UL * 1024UL * x; }

namespace Helpers {
	using enum fmt::color;

	static inline constexpr u32 signExtend32(u32 value, u32 startingSize) {
		auto temp = (s32)value;
		auto bitsToShift = 32 - startingSize;
		return (u32)(temp << bitsToShift >> bitsToShift);
	}

	static inline constexpr u16 signExtend16(u16 value, u32 startingSize) {
		auto temp = (s16)value;
		auto bitsToShift = 16 - startingSize;
		return (u16)(temp << bitsToShift >> bitsToShift);
	}

	template <typename ReturnType, typename V>
	constexpr ReturnType signExtend(V value) {
		return static_cast<ReturnType>(
			static_cast<typename std::make_signed<ReturnType>::type>(static_cast<typename std::make_signed<V>::type>(value)));
	}

	template <typename ReturnType, typename V>
	constexpr ReturnType zeroExtend(V value) {
		return static_cast<ReturnType>(
			static_cast<typename std::make_unsigned<ReturnType>::type>(static_cast<typename std::make_unsigned<V>::type>(value)));
	}

	template <typename... Args>
	[[noreturn]] static void panic(const char* fmt, const Args&... args) {
		fmt::print(fg(red), fmt, args...);
		fmt::print(fg(red), "Exiting\n");
		exit(1);
	}

	template <typename... Args>
	static void panic(bool condition, const char* fmt, const Args&... args) {
		if (condition) {
			fmt::print(fg(fmt::color::red), fmt, args...);
			fmt::print(fg(red), "Exiting\n");
			exit(1);
		}
	}

}  // namespace Helpers

namespace Log {
	using enum fmt::color;

	template <typename... Args>
	static void warn(fmt::format_string<Args...> fmt, const Args&... args) {
		fmt::print(fg(fmt::color::red), "Warn: ");
		fmt::print(fmt, std::forward<Args>(args)...);
	}

	template <typename... Args>
	static void info(fmt::format_string<Args...> fmt, const Args&... args) {
		fmt::print(fg(fmt::color::gray), "Info: ");
		fmt::print(fmt, std::forward<Args>(args)...);
	}

	template <typename... Args>
	static void debug(fmt::format_string<Args...> fmt, Args&&... args) {
		if constexpr (debugBuild()) {
			fmt::print(fg(fmt::color::green), "Debug: ");
			fmt::print(fmt, std::forward<Args>(args)...);
		}
	}

}  // namespace Log
