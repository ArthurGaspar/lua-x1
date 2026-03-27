#pragma once
#include <cstdint>
#include "config.h"

enum class EntityType : uint8_t {
    Character = 0,
    Projectile = 1
};

enum class StatusFlags : uint16_t {
    Clean        = 0,
    Stun        = 1 << 0,
    Root        = 1 << 1,
    KnockUp     = 1 << 2,
    Suppression = 1 << 3,
    Freeze      = 1 << 4,
    Paralysis   = 1 << 5,
    Burn        = 1 << 6,
    Confusion   = 1 << 7,
    Hypnosis    = 1 << 8
};

// (Max 8 buffs per entity)
struct ActiveBuff {
    uint32_t source_entity_id = 0;
    uint16_t buff_id = 0;
    int32_t remaining_ticks = 0;   
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
    uint16_t status_flags = 0;
    uint8_t active_buff_count = 0;
    ActiveBuff buffs[8];
    uint8_t party = 0; // team
    int32_t visionRadius = to_fixed(10.0f);

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
               status_flags == o.status_flags;
    }

    constexpr bool operator!=(EntityState const& o) const {
        return !(*this == o);
    }
};
#pragma pack(pop)