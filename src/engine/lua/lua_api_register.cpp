#include "lua_api_register.hpp"

#include <lua.h>
#include <lualib.h>

#include <SDL3/SDL.h>

#include "engine/constants.hpp"
#include "engine/engine.hpp"
#include "lua_transform.hpp"

static int l_screen_readPixel(lua_State *L)
{
    int x = (int)luaL_checkinteger(L, 1);
    int y = (int)luaL_checkinteger(L, 2);
    if (!g_eng.renderer)
    {
        lua_pushnil(L);
        return 1;
    }
    SDL_Rect rect{x, y, 1, 1};
    SDL_Surface *surf = SDL_RenderReadPixels(g_eng.renderer, &rect);
    if (!surf)
    {
        lua_pushnil(L);
        return 1;
    }
    Uint8 r = 0, green = 0, b = 0, a = 0;
    if (!SDL_ReadSurfacePixel(surf, 0, 0, &r, &green, &b, &a))
    {
        SDL_DestroySurface(surf);
        lua_pushnil(L);
        return 1;
    }
    SDL_DestroySurface(surf);
    lua_pushinteger(L, r);
    lua_pushinteger(L, green);
    lua_pushinteger(L, b);
    lua_pushinteger(L, a);
    return 4;
}

void eng_lua_register_draw(lua_State *L);
void eng_lua_register_input(lua_State *L);
void eng_lua_register_audio(lua_State *L);
void eng_lua_register_app(lua_State *L);
void eng_lua_register_runtime(lua_State *L);
void eng_lua_register_require(lua_State *L);
void eng_lua_register_editor(lua_State *L);
void eng_lua_register_editor_input(lua_State *L);

void eng_lua_register_apis(lua_State *L)
{
    eng_lua_register_draw(L);
    eng_lua_register_input(L);
    eng_lua_register_audio(L);

    lua_newtable(L);
    lua_pushinteger(L, SCREEN_W);
    lua_setfield(L, -2, "w");
    lua_pushinteger(L, SCREEN_H);
    lua_setfield(L, -2, "h");
    lua_setglobal(L, "screen");

    lua_getglobal(L, "screen");
    lua_pushcfunction(L, l_screen_readPixel, "screen.readPixel");
    lua_setfield(L, -2, "readPixel");
    lua_pop(L, 1);

    eng_lua_register_app(L);
    eng_lua_register_transform_metatable(L);
    eng_lua_register_runtime(L);
    eng_lua_register_editor(L);
    eng_lua_register_editor_input(L);
    eng_lua_register_require(L);
}
