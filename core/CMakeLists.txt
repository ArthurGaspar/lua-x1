# TODO: Move Engine functions to a shared library or create separate implementations
# 
# add_executable(moba_example
#     src/main.cpp
#     src/lua_bridge.cpp
#     src/lua_bridge.h
# )
# target_include_directories(moba_example PRIVATE
#     ${CMAKE_CURRENT_SOURCE_DIR}/src
#     ${CMAKE_SOURCE_DIR}/libs/lua/lua-5.4.8
# )
# target_link_libraries(moba_example PRIVATE lua)
# file(COPY ${CMAKE_SOURCE_DIR}/game/scripts DESTINATION ${CMAKE_BINARY_DIR}/game)

# ===== Deterministic Sim Demo =====
add_executable(deterministic_sim_demo
    src/deterministic_sim.cpp
    src/lua_bridge.cpp
    src/map.cpp
    src/physics.cpp
)

target_include_directories(deterministic_sim_demo PRIVATE
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