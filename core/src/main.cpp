#include <iostream>
#include "lua_bridge.h"

int main() {
    LuaBridge bridge; // creates instance
    if (!bridge.doFile("../../game/scripts/abilities/fireball_test.lua")) {
        std::cerr << "Failed to load lua script\n";
        return 1;
    }

    // Simulate a caster (entity id 42) casting Fireball at (10.2, 5.7)
    bridge.callCastFunction("cast", 42, 10.2, 5.7);

    std::cout << "Done.\n";
    return 0;
}