cmake_minimum_required(VERSION 3.10)
project(MOBAEngine)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_subdirectory(libs/lua/lua-5.4.8)
add_subdirectory(core)

target_include_directories(moba_example PRIVATE 
    ${CMAKE_CURRENT_SOURCE_DIR}/src
    ${CMAKE_SOURCE_DIR}/main/libs/lua/lua-5.4.8
)