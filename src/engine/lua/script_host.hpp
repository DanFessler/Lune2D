#pragma once

#include <ctime>
#include <string>

struct lua_State;

/// Create a Lua VM with all engine APIs and auto-discovered behaviors.
/// Does not load or execute any game script.
/// @param engineLuaDir Absolute path to engine Luau root (contains `editor/*.lua`).
/// @param projectLuaDir Absolute path to the loaded game project (contains `behaviors/`, `game/`, …).
lua_State *eng_create_lua_vm(const std::string &engineLuaDir, const std::string &projectLuaDir);

/// Clear `package.loaded` behavior modules, replace `_BEHAVIORS`, rescan `behaviors/*.lua`.
/// Keeps the same `lua_State` and does not reset the Scene.
void eng_reload_behaviors(lua_State *L,
                          const std::string &engineLuaDir,
                          const std::string &projectLuaDir);

bool eng_lua_call(lua_State *L, const char *fn, int nargs, int nret);

std::time_t eng_file_mtime(const std::string &path);

/// Updates `screen.w` / `screen.h` and `s_game_lu_w` / `s_game_lu_h`.
void eng_sync_lua_screen_size(lua_State *L, int w, int h);
