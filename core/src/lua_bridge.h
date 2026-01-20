#pragma once
#include <lua.hpp>
#include <string>

struct LuaBridge {
    lua_State *L;   // Pointer for Lua state (holds context)
    LuaBridge();    // Constructor
    ~LuaBridge();   // Destructor

    // Load and execute Lua file
    bool doFile(const std::string &path);
    // Specific Lua function for test (fireball-test)
    bool callCastFunction(const std::string &fnName, int caster_id, double target_x, double target_y);

    // Register core game functions to Lua
    void registerCoreBindings();

    //  C functions exported to Lua
    static int l_GetPosition(lua_State* L);
    static int l_SetMovement(lua_State* L);
    static int l_ApplyDamage(lua_State* L);
    static int l_ApplyKnockback(lua_State* L);
    static int l_SpawnProjectile(lua_State* L);
    static int l_RegisterTimer(lua_State* L);

    // helpers for reading tables
    static bool checkFieldNumber(lua_State* L, int idx, const char* key, double &out);
    static bool checkFieldInt(lua_State* L, int idx, const char* key, int &out);
};