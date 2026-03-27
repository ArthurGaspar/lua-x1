# ===== Deterministic Sim Demo =====
add_executable(deterministic_sim_demo
    src/deterministic_sim.cpp
    src/lua_bridge.cpp
    src/map.cpp
    src/physics.cpp
    src/ui_bridge.cpp
)

target_include_directories(deterministic_sim_demo PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${CMAKE_CURRENT_SOURCE_DIR}/src
    ${CMAKE_SOURCE_DIR}/libs/lua/lua-5.4.8/src
)

target_link_libraries(deterministic_sim_demo PRIVATE lua)

# Link pthread only on non-Windows platforms
if(NOT WIN32)
    target_link_libraries(deterministic_sim_demo PRIVATE pthread)
endif()

# Copy game scripts to demo output directory
file(COPY ${CMAKE_SOURCE_DIR}/game/scripts DESTINATION ${CMAKE_CURRENT_BINARY_DIR}/game)

# ===== GameEngine DLL (for C# interop) =====
add_library(GameEngine SHARED
    src/deterministic_sim.cpp
    src/lua_bridge.cpp
    src/map.cpp
    src/physics.cpp
    src/ui_bridge.cpp
)

target_include_directories(GameEngine PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${CMAKE_CURRENT_SOURCE_DIR}/src
    ${CMAKE_SOURCE_DIR}/libs/lua/lua-5.4.8/src
)

target_include_directories(GameEngine PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_link_libraries(GameEngine PRIVATE lua)

# Link pthread only on non-Windows platforms
if(NOT WIN32)
    target_link_libraries(GameEngine PRIVATE pthread)
endif()

# Copy game scripts to DLL output directory
file(COPY ${CMAKE_SOURCE_DIR}/game/scripts DESTINATION ${CMAKE_CURRENT_BINARY_DIR}/game)