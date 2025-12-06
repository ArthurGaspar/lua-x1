#include "lua_bridge.h"
#include <iostream>

// lua_pop is (L,n) (n >= 1)
// other operations read specific values (-1 is the newest, 1 is the oldest = not so usual)

LuaBridge::LuaBridge() {
    L = luaL_newstate();
    luaL_openlibs(L); // open standard libs (optionally restrict in prod)
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