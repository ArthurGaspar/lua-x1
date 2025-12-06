function cast(caster_id, target_x, target_y)
    print(string.format("[Lua] %d casts Fireball at (%.2f, %.2f)", caster_id, target_x, target_y))

    local ability = {
        name = "FireballTest",
        damage = 120,
        radius = 1.5,
        speed = 360.0,
        tx = target_x,
        ty = target_y
    }

    return ability
end