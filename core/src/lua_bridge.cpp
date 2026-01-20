#include "lua_bridge.h"
#include <iostream>
#include <vector>
#include <string>
#include <cmath>

// ---- Engine integration stubs (MUST BE IMPLEMENTED) ----
// Replace them with the engine functions from the deterministic_sim.cpp.
// They are declared extern here can be implemented in engine code.

extern bool Engine_GetPosition(int entity_id, double &out_x, double &out_y);
extern bool Engine_SetMovement(int entity_id, double vx, double vy);
extern bool Engine_ApplyDamage(int source_id, int target_id, int amount, const char* damage_type);
extern bool Engine_ApplyKnockback(int source_id, int target_id, double dir_x, double dir_y, double force, double duration);

// returns new projectile entity id (>0) or -1 on error
extern int Engine_SpawnProjectile(int caster_id, double spawn_x, double spawn_y, double dir_x, double dir_y, double speed, double radius, double life_time, const char* on_hit_cb);

extern bool Engine_RegisterTimer(const char* callback_name, double delay_seconds, int repeat_count);

// -------------------------------------------------------------

// lua_pop is (L,n) (n >= 1)
// other operations read specific values (-1 is the newest, 1 is the oldest = not so usual)

LuaBridge::LuaBridge() {
    L = luaL_newstate();
    luaL_openlibs(L); // open standard libs (optionally restrict in prod)

    // register bindings
    registerCoreBindings();
}

LuaBridge::~LuaBridge() {
    if (L) lua_close(L);
}

bool LuaBridge::doFile(const std::string &path) {
    if (luaL_dofile(L, path.c_str()) != LUA_OK) {
        std::cerr << "[Lua] Error loading file: " << lua_tostring(L, -1) << std::endl;
        lua_pop(L,1);
        return false;
    }
    return true;
}

bool LuaBridge::callCastFunction(const std::string &fnName, int caster_id, double target_x, double target_y) {
    lua_getglobal(L, fnName.c_str()); // push function
    if (!lua_isfunction(L, -1)) {
        lua_pop(L,1);
        std::cerr << "[Lua] Function not found: " << fnName << std::endl;
        return false;
    }

    lua_pushinteger(L, caster_id);
    lua_pushnumber(L, target_x);
    lua_pushnumber(L, target_y);

    // call function with 3 args, 1 result and no custom error handler
    // lua p_call(L, n-args, n-results, errfunc)
    if (lua_pcall(L, 3, 1, 0) != LUA_OK) {
        std::cerr << "[Lua] Error calling function: " << lua_tostring(L, -1) << std::endl;
        lua_pop(L,1);
        return false;
    }

    // Now top of stack is the returned Lua table (or something)
    if (!lua_istable(L, -1)) {
        std::cerr << "[Lua] Expected table result from cast, got something else." << std::endl;
        lua_pop(L,1);
        return false;
    }

    // -- EXAMPLE: Reading "damage" of the ability --
    lua_getfield(L, -1, "damage");
    if (!lua_isnumber(L, -1)) {
        lua_pop(L,2); // pops the field value + the table
        std::cerr << "[Lua] ability.damage missing or not a number\n";
        return false;
    }
    int damage = (int)lua_tointeger(L, -1);
    lua_pop(L,1);

    std::cout << "[C++] Lua reported damage = " << damage << std::endl;

    lua_pop(L,1);
    return true;

}

// ------------------- Helpers --------------------

bool LuaBridge::checkFieldNumber(lua_State* L, int idx, const char* key, double &out) {
    lua_getfield(L, idx, key);
    if (!lua_isnumber(L, -1)) {
        lua_pop(L,1);
        return false;
    }
    out = lua_tonumber(L, -1);
    lua_pop(L,1);
    return true;
}

bool LuaBridge::checkFieldInt(lua_State* L, int idx, const char* key, int &out) {
    lua_getfield(L, idx, key);
    if (!lua_isinteger(L, -1)) {
        lua_pop(L,1);
        return false;
    }
    out = (int)lua_tointeger(L, -1);
    lua_pop(L,1);
    return true;
}

// ------------------- Binding registration --------------------

void LuaBridge::registerCoreBindings() {
    lua_register(L, "GetPosition", LuaBridge::l_GetPosition);
    lua_register(L, "SetMovement", LuaBridge::l_SetMovement);
    lua_register(L, "ApplyDamage", LuaBridge::l_ApplyDamage);
    lua_register(L, "ApplyKnockback", LuaBridge::l_ApplyKnockback);
    lua_register(L, "SpawnProjectile", LuaBridge::l_SpawnProjectile);
    lua_register(L, "RegisterTimer", LuaBridge::l_RegisterTimer);

    // (Optionally) add a small helper table
    // e.g. could create Game API table: game.GetPosition(...) etc.
}

// ------------------- Binding implementations --------------------

// GetPosition(entity_id) -> x, y
int LuaBridge::l_GetPosition(lua_State* L) {
    if (!lua_isinteger(L, 1)) {
        lua_pushstring(L, "GetPosition: expected integer entity_id");
        lua_error(L);
        return 0;
    }
    int entity_id = (int)lua_tointeger(L, 1);
    double x = 0.0, y = 0.0;
    bool ok = Engine_GetPosition(entity_id, x, y);
    if (!ok) {
        lua_pushnil(L);
        lua_pushnil(L);
        return 2;
    }
    lua_pushnumber(L, x);
    lua_pushnumber(L, y);
    return 2;
}

// SetMovement(entity_id, vx, vy)
int LuaBridge::l_SetMovement(lua_State* L) {
    if (!lua_isinteger(L, 1) || !lua_isnumber(L, 2) || !lua_isnumber(L, 3)) {
        lua_pushstring(L, "SetMovement: expected (int entity_id, number vx, number vy)");
        lua_error(L);
        return 0;
    }
    int entity_id = (int)lua_tointeger(L, 1);
    double vx = lua_tonumber(L, 2);
    double vy = lua_tonumber(L, 3);
    bool ok = Engine_SetMovement(entity_id, vx, vy);
    lua_pushboolean(L, ok ? 1 : 0);
    return 1;
}

// ApplyDamage(source_id, target_id, amount, damage_type)
int LuaBridge::l_ApplyDamage(lua_State* L) {
    int args = lua_gettop(L);
    if (args < 3 || !lua_isinteger(L,1) || !lua_isinteger(L,2) || !lua_isnumber(L,3)) {
        lua_pushstring(L, "ApplyDamage: expected (int source_id, int target_id, number amount, optional string damage_type)");
        lua_error(L);
        return 0;
    }
    int source = (int)lua_tointeger(L,1);
    int target = (int)lua_tointeger(L,2);
    int amount = (int)lua_tonumber(L,3);
    const char* dtype = "physical";
    if (args >= 4 && lua_isstring(L,4)) dtype = lua_tostring(L,4);
    bool ok = Engine_ApplyDamage(source, target, amount, dtype);
    lua_pushboolean(L, ok ? 1 : 0);
    return 1;
}

// ApplyKnockback(source_id, target_id, dir_x, dir_y, force, duration)
int LuaBridge::l_ApplyKnockback(lua_State* L) {
    if (lua_gettop(L) < 6 ||
        !lua_isinteger(L,1) || !lua_isinteger(L,2) ||
        !lua_isnumber(L,3) || !lua_isnumber(L,4) ||
        !lua_isnumber(L,5) || !lua_isnumber(L,6)) {
        lua_pushstring(L, "ApplyKnockback: expected (int source_id, int target_id, number dir_x, number dir_y, number force, number duration)");
        lua_error(L);
        return 0;
    }
    int source = (int)lua_tointeger(L,1);
    int target = (int)lua_tointeger(L,2);
    double dx = lua_tonumber(L,3);
    double dy = lua_tonumber(L,4);
    double force = lua_tonumber(L,5);
    double duration = lua_tonumber(L,6);

    // normalize dir
    double len = std::sqrt(dx*dx + dy*dy);
    if (len == 0.0) { dx = 1.0; dy = 0.0; len = 1.0; }
    dx /= len; dy /= len; // unit vectors

    bool ok = Engine_ApplyKnockback(source, target, dx, dy, force, duration);
    lua_pushboolean(L, ok ? 1 : 0);
    return 1;
}

// SpawnProjectile(params_table) -> projectile_id (int) or nil
int LuaBridge::l_SpawnProjectile(lua_State* L) {
    if (!lua_istable(L, 1)) {
        lua_pushstring(L, "SpawnProjectile: expected table argument");
        lua_error(L);
        return 0;
    }

    // [REQUIRED] caster
    lua_getfield(L, 1, "caster");
    if (!lua_isinteger(L, -1)) {
        lua_pop(L,1);
        lua_pushstring(L, "SpawnProjectile: 'caster' integer field required");
        lua_error(L);
        return 0;
    }
    int caster = (int)lua_tointeger(L, -1);
    lua_pop(L,1);

    // spawn pos.x, pos.y
    double spawn_x = 0.0, spawn_y = 0.0;
    lua_getfield(L, 1, "pos");
    if (lua_istable(L, -1)) {
        lua_getfield(L, -1, "x");
        if (lua_isnumber(L, -1)) spawn_x = lua_tonumber(L, -1);
        lua_pop(L,1);
        lua_getfield(L, -1, "y");
        if (lua_isnumber(L, -1)) spawn_y = lua_tonumber(L, -1);
        lua_pop(L,1);
    }
    lua_pop(L,1);

    // [REQUIRED] dir OR target
    // dir.x, dir.y
    double dir_x = 0.0, dir_y = 0.0;
    bool have_dir = false;
    lua_getfield(L, 1, "dir");
    if (lua_istable(L, -1)) {
        lua_getfield(L, -1, "x");
        if (lua_isnumber(L, -1)) dir_x = lua_tonumber(L, -1);
        lua_pop(L,1);
        lua_getfield(L, -1, "y");
        if (lua_isnumber(L, -1)) dir_y = lua_tonumber(L, -1);
        lua_pop(L,1);
        have_dir = true;
    }
    lua_pop(L,1);
    // target_pos (optional) - if dir not provided use target_pos - spawn->target vector
    bool have_target_pos = false;
    double target_x = 0.0, target_y = 0.0;
    lua_getfield(L, 1, "target_pos");
    if (lua_istable(L, -1)) {
        lua_getfield(L, -1, "x");
        if (lua_isnumber(L, -1)) target_x = lua_tonumber(L, -1);
        lua_pop(L,1);
        lua_getfield(L, -1, "y");
        if (lua_isnumber(L, -1)) target_y = lua_tonumber(L, -1);
        lua_pop(L,1);
        have_target_pos = true;
    }
    lua_pop(L,1);

    // [REQUIRED] speed
    double speed = 0.0;
    if (!checkFieldNumber(L, 1, "speed", speed)) {
        lua_pushstring(L, "SpawnProjectile: 'speed' required (number)");
        lua_error(L);
        return 0;
    }

    // radius, lifetime
    double radius = 0.0;
    checkFieldNumber(L, 1, "radius", radius);
    double life_time = 0.0;
    checkFieldNumber(L, 1, "life_time", life_time);

    // on_hit callback name (string)
    std::string on_hit;
    lua_getfield(L, 1, "on_hit");
    if (lua_isstring(L, -1)) {
        on_hit = lua_tostring(L, -1);
    }
    lua_pop(L,1);

    // Cases where optionals are not available
    // 1. If spawn pos not specified, try to use caster position
    if (spawn_x == 0.0 && spawn_y == 0.0) {
        double sx=0.0, sy=0.0;
        if (Engine_GetPosition(caster, sx, sy)) {
            spawn_x = sx;
            spawn_y = sy;
        }
    }
    // 2. If dir not provided but target_pos is provided, compute dir
    if (!have_dir && have_target_pos) {
        double dx = target_x - spawn_x;
        double dy = target_y - spawn_y;
        double len = std::sqrt(dx*dx + dy*dy);
        if (len == 0.0) { dir_x = 1.0; dir_y = 0.0; }
        else { dir_x = dx/len; dir_y = dy/len; }
    }

    // final normalization of SpawnProjectiles
    double dlen = std::sqrt(dir_x*dir_x + dir_y*dir_y);
    if (dlen == 0.0) { dir_x = 1.0; dir_y = 0.0; }
    else { dir_x /= dlen; dir_y /= dlen; }

    // Call engine to create projectile
    const char* on_hit_cstr = on_hit.empty() ? nullptr : on_hit.c_str();
    int proj_id = Engine_SpawnProjectile(caster, spawn_x, spawn_y, dir_x, dir_y, speed, radius, life_time, on_hit_cstr);
    if (proj_id <= 0) {
        lua_pushnil(L);
        lua_pushstring(L, "Failed to spawn projectile");
        return 2;
    }

    lua_pushinteger(L, proj_id);
    return 1;
}

// RegisterTimer(callback_name, delay_seconds, repeat_count)
int LuaBridge::l_RegisterTimer(lua_State* L) {
    if (!lua_isstring(L,1) || !lua_isnumber(L,2)) {
        lua_pushstring(L, "RegisterTimer: expected (string callback_name, number delay_seconds, optional int repeat_count)");
        lua_error(L);
        return 0;
    }
    const char* cb = lua_tostring(L,1);
    double delay = lua_tonumber(L,2);
    int repeat = 1;
    if (lua_gettop(L) >= 3 && lua_isinteger(L,3)) repeat = (int)lua_tointeger(L,3);

    bool ok = Engine_RegisterTimer(cb, delay, repeat);
    lua_pushboolean(L, ok ? 1 : 0);
    return 1;
}