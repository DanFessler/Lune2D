// Minimal definition for tests that link `scene.cpp` without the full Lua runtime.
#include "engine/lua/lua_runtime.hpp"

lua_State *g_eng_lua_vm = nullptr;
