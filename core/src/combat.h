#pragma once
#include <cstdint>

enum class DamageType : uint8_t {
    Physical = 1,
    Magical = 2,
    Absolute = 3
};

inline int32_t CalculateFinalDamage(int32_t raw_damage, int32_t resistance, DamageType type) {
    if (type == DamageType::Absolute) return raw_damage;
    
    // prevent negative resistance
    int32_t effective_res = resistance > 0 ? resistance : 0; 
    
    int64_t numerator = static_cast<int64_t>(raw_damage) * 100;
    int32_t denominator = 100 + effective_res;
    
    return static_cast<int32_t>(numerator / denominator);
}