#pragma once

#include <lua.h>
#include <lualib.h>

#include <SDL3/SDL.h>

inline SDL_Color eng_lua_color(lua_State* L, int r, int g, int b, int a) {
    return {
        (Uint8)luaL_optinteger(L, r, 255),
        (Uint8)luaL_optinteger(L, g, 255),
        (Uint8)luaL_optinteger(L, b, 255),
        (Uint8)luaL_optinteger(L, a, 255),
    };
}
