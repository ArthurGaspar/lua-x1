-- NEW --

local FIREBALL_DAMAGE = 120
local FIREBALL_SPEED = 360.0
local FIREBALL_RADIUS = 1.5
local FIREBALL_LIFETIME = 4.0

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

    local proj_id, err = SpawnProjectile({
        caster = caster_id, -- req
        pos = {x = sx, y = sy},
        dir = {x = dx, y = dy}, -- req (could be target instead)
        speed = FIREBALL_SPEED, -- req
        radius = FIREBALL_RADIUS,
        life_time = FIREBALL_LIFETIME,
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