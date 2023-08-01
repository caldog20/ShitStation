#pragma once
#include <bitset>
#include <cstdint>
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

using i8 = std::int8_t;
using i16 = std::int16_t;
using i32 = std::int32_t;
using i64 = std::int64_t;

using Cycles = u64;

constexpr size_t operator""_KB(unsigned long long int x) { return 1024ULL * x; }
constexpr size_t operator""_MB(unsigned long long int x) { return 1024_KB * x; }

namespace Helpers {

static constexpr bool debugBuild() {
#ifdef NDEBUG
    return false;
#endif
    return true;
}

template <typename ReturnType, typename V>
constexpr ReturnType signExtend(V value) {
    return static_cast<ReturnType>(static_cast<typename std::make_signed<ReturnType>::type>(static_cast<typename std::make_signed<V>::type>(value)));
}

template <typename ReturnType, typename V>
constexpr ReturnType zeroExtend(V value) {
    return static_cast<ReturnType>(static_cast<typename std::make_unsigned<ReturnType>::type>(static_cast<typename std::make_unsigned<V>::type>(value)
    ));
}

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

template <typename... Args>
[[noreturn]] static void panic(const char* fmt, const Args&... args) {
    using enum fmt::color;
    fmt::print(fg(red), fmt, args...);
    fmt::print(fg(red), "Exiting\n");
    exit(1);
}

template <typename... Args>
static void panic(bool condition, const char* fmt, const Args&... args) {
    using enum fmt::color;
    if (condition) {
        fmt::print(fg(red), fmt, args...);
        fmt::print(fg(red), "Exiting\n");
        exit(1);
    }
}

static inline void debugAssert(bool condition, std::string message) {
    if constexpr (debugBuild()) {
        if (!condition) {
            panic("Debug assertion failed: {}\n", message.c_str());
        }
    }
}

static inline bool isBitSet(u32 value, int bit) { return ((value >> bit) & 1) != 0; }

template <typename T>
static inline void setBit(T& value, int bit) {
    value |= (1 << bit);
}

}  // namespace Helpers
