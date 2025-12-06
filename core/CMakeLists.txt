add_executable(moba_example
    src/main.cpp
    src/lua_bridge.cpp
    src/lua_bridge.h
)

# includes directories of headers
target_include_directories(moba_example PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/src
    ${CMAKE_SOURCE_DIR}/libs/lua/lua-5.4.8
)

target_link_libraries(moba_example PRIVATE lua)
file(COPY ${CMAKE_SOURCE_DIR}/game/scripts DESTINATION ${CMAKE_BINARY_DIR}/game)