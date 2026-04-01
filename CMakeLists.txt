cmake_minimum_required(VERSION 3.20)
project(Asteroids)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(SDL3 REQUIRED)

# Lua (Homebrew installs headers under include/lua on Apple Silicon)
find_path(LUA_INCLUDE_DIR lua.h
    PATHS /opt/homebrew/include/lua /usr/local/include/lua /usr/include/lua5.4)
find_library(LUA_LIBRARY NAMES lua lua5.5 lua5.4
    PATHS /opt/homebrew/lib /usr/local/lib)

if(NOT LUA_INCLUDE_DIR OR NOT LUA_LIBRARY)
    message(FATAL_ERROR "Lua not found")
endif()

add_executable(asteroids src/main.cpp)

target_include_directories(asteroids PRIVATE ${LUA_INCLUDE_DIR})
target_link_libraries(asteroids PRIVATE SDL3::SDL3 ${LUA_LIBRARY})
