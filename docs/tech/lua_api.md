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
