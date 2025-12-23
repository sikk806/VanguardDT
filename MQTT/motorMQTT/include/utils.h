#pragma once
#include <string>
#include <chrono>
#include <cstdint>

namespace utils {

template<typename T>
inline T clamp(T v, T lo, T hi) {
    return (v < lo) ? lo : (v > hi) ? hi : v;
}

int64_t now_unix_ms();
std::string iso8601_utc_now(); // "2025-12-08T12:40:00.123Z"

} // namespace utils
