#include "editor_state.hpp"
#include "editor_pick.hpp"
#include "scene.hpp"
#include "webview_host.hpp"

#include <lua.h>
#include <lualib.h>

#include "engine/engine.hpp"

static int l_editor_playState(lua_State *L)
{
    switch (eng_editor_sim_ui_state())
    {
    case EngSimUiState::Stopped:
        lua_pushstring(L, "stopped");
        break;
    case EngSimUiState::Playing:
        lua_pushstring(L, "playing");
        break;
    case EngSimUiState::Paused:
        lua_pushstring(L, "paused");
        break;
    }
    return 1;
}

static int l_editor_getSelectedEntityId(lua_State *L)
{
    lua_pushinteger(L, (lua_Integer)eng_editor_selected_entity());
    return 1;
}

static int l_editor_setSelectedEntity(lua_State *L)
{
    uint32_t id = 0;
    if (!lua_isnil(L, 1) && !lua_isnoneornil(L, 1))
        id = (uint32_t)luaL_checkinteger(L, 1);
    eng_editor_set_selected_entity(id);
    webview_host_notify_selected_entity(id);
    return 0;
}

static int l_editor_pickEntityAt(lua_State *L)
{
    float lx = (float)luaL_checknumber(L, 1);
    float ly = (float)luaL_checknumber(L, 2);
    float hitR = (float)luaL_optnumber(L, 3, 18.0);
    uint32_t id = eng_editor_pick_entity_at_lu(g_scene, lx, ly, hitR);
    lua_pushinteger(L, (lua_Integer)id);
    return 1;
}

void eng_lua_register_editor(lua_State *L)
{
    lua_newtable(L);
    lua_pushcfunction(L, l_editor_playState, "editor.playState");
    lua_setfield(L, -2, "playState");
    lua_pushcfunction(L, l_editor_getSelectedEntityId, "editor.getSelectedEntityId");
    lua_setfield(L, -2, "getSelectedEntityId");
    lua_pushcfunction(L, l_editor_setSelectedEntity, "editor.setSelectedEntity");
    lua_setfield(L, -2, "setSelectedEntity");
    lua_pushcfunction(L, l_editor_pickEntityAt, "editor.pickEntityAt");
    lua_setfield(L, -2, "pickEntityAt");
    lua_setglobal(L, "editor");
}
