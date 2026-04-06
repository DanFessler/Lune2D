#include "lua_api_register.hpp"

#include <lua.h>

#include "engine/constants.hpp"
#include "lua_transform.hpp"

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

    eng_lua_register_app(L);
    eng_lua_register_transform_metatable(L);
    eng_lua_register_runtime(L);
    eng_lua_register_editor(L);
    eng_lua_register_editor_input(L);
    eng_lua_register_require(L);
}
