#include <lua.h>
#include <lualib.h>

#include <cstring>

#include <SDL3/SDL.h>

#include "engine/engine.hpp"

static int l_input_down(lua_State* L) {
    const char* name = luaL_checkstring(L, 1);
    SDL_Scancode sc  = SDL_SCANCODE_UNKNOWN;
    if (!strcmp(name, "left"))
        sc = SDL_SCANCODE_LEFT;
    else if (!strcmp(name, "right"))
        sc = SDL_SCANCODE_RIGHT;
    else if (!strcmp(name, "up"))
        sc = SDL_SCANCODE_UP;
    else if (!strcmp(name, "down"))
        sc = SDL_SCANCODE_DOWN;
    else if (!strcmp(name, "space"))
        sc = SDL_SCANCODE_SPACE;
    else if (!strcmp(name, "a"))
        sc = SDL_SCANCODE_A;
    else if (!strcmp(name, "d"))
        sc = SDL_SCANCODE_D;
    else if (!strcmp(name, "w"))
        sc = SDL_SCANCODE_W;
    else if (!strcmp(name, "r"))
        sc = SDL_SCANCODE_R;
    else if (!strcmp(name, "escape"))
        sc = SDL_SCANCODE_ESCAPE;
    lua_pushboolean(L, sc != SDL_SCANCODE_UNKNOWN && g_eng.keys && g_eng.keys[sc]);
    return 1;
}

void eng_lua_register_input(lua_State* L) {
    lua_newtable(L);
    lua_pushcfunction(L, l_input_down, "input.down");
    lua_setfield(L, -2, "down");
    lua_setglobal(L, "input");
}
