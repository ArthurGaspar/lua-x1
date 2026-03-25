#pragma once
#include <cstdint>

enum class EntityType : uint8_t {
    Character = 0,
    Projectile = 1
};

#pragma pack(push, 1)
struct EntityState {
    uint32_t id = 0;
    EntityType type = EntityType::Character;
    int32_t pos_x = 0; // fixed-point
    int32_t pos_y = 0;
    int32_t vel_x = 0; // fixed-point units PER SIMULATION TICK
    int32_t vel_y = 0;
    int32_t health = 0;
    int32_t radius = 0;
    int32_t lifetime_ticks = -1; // -1 = infinite
    uint8_t flags = 0;

    constexpr EntityState() = default;

    // Callbacks (simplified for now)
    // ex.: std::string on_hit_callback; 

    constexpr bool operator==(EntityState const& o) const {
        return id == o.id &&
               type == o.type &&
               pos_x == o.pos_x &&
               pos_y == o.pos_y &&
               vel_x == o.vel_x &&
               vel_y == o.vel_y &&
               health == o.health &&
               radius == o.radius &&
               lifetime_ticks == o.lifetime_ticks &&
               flags == o.flags;
    }

    constexpr bool operator!=(EntityState const& o) const {
        return !(*this == o);
    }
};
#pragma pack(pop)