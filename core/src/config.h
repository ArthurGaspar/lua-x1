#pragma once
#include <cstdint>

// ---------- Config ----------
constexpr int SERVER_TICK_RATE = 30; // 30t/s
constexpr uint64_t TICK_NS = 1000000000ull / SERVER_TICK_RATE; // 33,333,333 ns (approx)
constexpr int32_t POS_SCALE = 1000; // fixed-point scale: 1.0 world unit = 1000 units
constexpr size_t MAX_CLIENT_INPUT_QUEUE = 256;

// ---------- Fixed-point helpers ----------
inline int32_t to_fixed(float world_units) {
    return static_cast<int32_t>(world_units * POS_SCALE);
}
inline float to_world(int32_t fixed) {
    return static_cast<float>(fixed) / POS_SCALE;
}