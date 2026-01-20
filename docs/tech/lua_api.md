# Lua API — Game scripting (v1)

This document describes the first version of the Lua runtime API used by abilities and gameplay scripts.

## Design principles

- **Deterministic runtime**: The engine's authoritative simulation uses integer entity IDs to reference runtime objects. All state-changing functions (damage, movement, spawn) are *engine functions* that mutate simulation state deterministically.
- **Simple surface for designers**: Ability scripts return plain Lua tables describing immediate results (e.g. an ability cast returning a projectile descriptor) or call engine functions directly.
- **Tooling friendliness**: Tools (CharAbilityEditor, SimPreview) may provide Lua-side userdata wrappers (syntactic sugar) for convenience. Those wrappers should resolve to integer IDs before invoking engine functions.
- **Tick semantics**: All engine-visible state changes must be applied inside the simulation tick or in code called from a tick. Avoid storing engine pointers/objects inside Lua across ticks.

---

## Namespace & global functions

All core functions are exposed as global functions (for ease of use in scripts):

### `GetPosition(entity_id) -> x, y`
Return the current world-position of `entity_id` as two numbers (floating point world units).

**Example**
```lua
local x, y = GetPosition(1001)
```

### `SetMovement(entity_id, vx, vy)`

Set the movement velocity (units/sec) or movement vector for the entity. This schedules the velocity to be applied in the next physics update.

**Example**

```lua
SetMovement(1001, 200.0, 0.0) -- move right at 200 u/s
```

> Implementation note: `vx, vy` are engine units/sec. Engine physics should apply them deterministically in the tick.

---

### `ApplyDamage(source_id, target_id, amount, damage_type)`

Applies `amount` damage from `source` to `target`. `damage_type` is optional string: `"physical"`, `"magical"`, `"absolute"`. The engine will apply armor/resist calculations.

**Return**: boolean `success`.

**Example**

```lua
ApplyDamage(caster_id, target_id, 120, "magical")
```

---

### `ApplyKnockback(source_id, target_id, dir_x, dir_y, force, duration)`

Applies a knockback to `target`. Direction `(dir_x, dir_y)` should be normalized (or engine will normalize). `force` is units (translation magnitude) and `duration` is seconds.

**Example**

```lua
ApplyKnockback(caster_id, target_id, nx, ny, 250.0, 0.5)
```

---

### `SpawnProjectile(params_table) -> projectile_id (int) or nil, error`

Spawn a projectile; `params_table` must be a table with these fields (recommended fields):

* `caster_id` (int) — entity id (required)
* `spawn` (table `{x=..., y=...}`) — spawn position (optional, engine may fallback to `GetPosition(caster)`)
* `dir OR target` (required)
  * `dir` (table `{x=..., y=...}`) — normalized direction vector
  * `target` (table `{x=..., y=...}`) — target point
* `speed` (number) — units/sec (required)
* `radius` (number) — collision radius, world units (optional)
* `life_time` (number) — seconds until auto-despawn (optional)
* `on_hit_cb` (string) — name of a Lua callback (engine will call the global function `on_hit` with signature below); alternatively, engine can generate events consumed by scripts
* `tags` (table) — optional metadata

**Return**: integer projectile id on success.

**Example**

```lua
local proj_id, err = SpawnProjectile({
  caster = caster_id,
  pos = {x = sx, y = sy},
  target_pos = {x = tx, y = ty},
  speed = 360.0,
  radius = 0.5,
  life_time = 6.0,
  on_hit = "OnProjectileHit_Fireball"
})
```

The engine will create a projectile entity and run collision checks each tick. If `on_hit` is set, the engine will call the global Lua function:

```lua
function OnProjectileHit_Fireball(projectile_id, target_entity_id)
  -- implement behavior
  -- just an example of function
end
```

---

## Ability script pattern

A minimal ability script must provide the `cast` function. The engine calls `cast(caster_id, target_x, target_y)` when the player casts. `cast` should either call engine functions (preferred) or return a table describing results.

**Example pattern**

```lua
-- abilities/fireball_test.lua
function cast(caster_id, target_x, target_y)
  local sx, sy = GetPosition(caster_id)
  local dx, dy = (target_x - sx), (target_y - sy)
  local len = math.sqrt(dx*dx + dy*dy)
  if len == 0 then dx,dy = 1,0 end
  dx,dy = dx/len, dy/len

  local proj_id = SpawnProjectile({
    caster = caster_id,
    pos = {x = sx, y = sy},
    dir = {x = dx, y = dy},
    speed = 360.0,
    radius = 0.5,
    life_time = 4.0,
    on_hit = "OnProjectileHit_Fireball"
  })

  return {ok = true, projectile = proj_id}
end

function OnProjectileHit_Fireball(projectile_id, target_id)
  ApplyDamage(GetProjectileCaster(projectile_id), target_id, 120, "magical")
end
```

> Note: `GetProjectileCaster` is a convenience engine function (optional) returning the caster id of a projectile.

---

## Determinism & best practices

* Avoid allocation or storing engine object pointers in Lua across ticks. Use IDs and re-query the engine if needed.
* Keeping side effects explicit: calls such as `ApplyDamage` are instantaneous and will be applied deterministically in the simulation tick.
* If needed to keep temporary Lua-only state between ticks (for tools/prototyping), use local Lua tables that do not reference raw engine pointers.

---

## Tooling / Userdata wrappers (optional)

Tools or editor-side code may create friendly Lua userdata objects (e.g. `Entity` objects with methods). Those wrappers **must** convert method calls to the integer-ID API under the hood.

Example wrapper (tool-side only):

```lua
-- pseudo-code inside tool:
local ent = CreateEntityWrapper(1001) -- userdata
ent:GetPosition() -- internally calls GetPosition(1001)
ent:ApplyDamage(source, 120) -- calls ApplyDamage(source_id, 1001, 120)
```

Should not use this pattern on authoritative server code unless guarantee the wrapper never stores engine pointers across ticks.

---