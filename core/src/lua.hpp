// lua.hpp
// Lua header file for C++
// <<extern "C">> not supplied automatically because Lua also compiles as C++

extern "C" {
#include "../../libs/lua/lua-5.4.8/src/lua.h"
#include "../../libs/lua/lua-5.4.8/src/lualib.h"
#include "../../libs/lua/lua-5.4.8/src/lauxlib.h"
}