#include <lua.h>

#include "engine/engine.hpp"

static int l_app_quit(lua_State* /*L*/) {
    g_eng.quit = true;
    return 0;
}

void eng_lua_register_app(lua_State* L) {
    lua_newtable(L);
    lua_pushcfunction(L, l_app_quit, "app.quit");
    lua_setfield(L, -2, "quit");
    lua_setglobal(L, "app");
}
