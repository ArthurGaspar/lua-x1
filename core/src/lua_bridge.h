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
};