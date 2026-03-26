function cast(caster_id, target_x, target_y)
    print(string.format("[Lua] %d casts Fireball at (%.2f, %.2f)", caster_id, target_x, target_y))

    local sx, sy = GetPosition(caster_id)
    if not sx then -- works for both sx and sy
        -- fallback
        sx = 0; sy = 0
    end

    -- compute direction
    local dx = target_x - sx
    local dy = target_y - sy
    local len = math.sqrt(dx*dx + dy*dy)
    if len == 0 then dx, dy = 1, 0; len = 1 end
    dx, dy = dx/len, dy/len

    local getDmg = Engine_GetAbilityStat("fireball_test", "damage")
    local getSpeed = Engine_GetAbilityStat("fireball_test", "speed")
    local getRadius = Engine_GetAbilityStat("fireball_test", "radius")
    local getLifetime = Engine_GetAbilityStat("fireball_test", "lifetime")

    local proj_id, err = SpawnProjectile({
        caster = caster_id, -- req
        pos = {x = sx, y = sy},
        dir = {x = dx, y = dy}, -- req (could be target instead)
        speed = getSpeed, -- req
        radius = getRadius,
        life_time = getLifetime,
        on_hit = "OnProjectileHit_Fireball"
    })

    if not proj_id then
        print("[Lua] Failed to spawn projectile:", err)
        return { ok = false, err = err }
    end

    return { ok = true, projectile = proj_id }
end

function OnProjectileHit_Fireball(projectile_id, target_id)
    local caster = GetProjectileCaster and GetProjectileCaster(projectile_id) or nil
    if caster == nil then
        -- best-effort: fallback to 0
        caster = 0
    end
    print(string.format("[Lua] projectile %d hit entity %d -- applying damage %d", projectile_id, target_id, FIREBALL_DAMAGE))
    ApplyDamage(caster, target_id, FIREBALL_DAMAGE, "magical")
end