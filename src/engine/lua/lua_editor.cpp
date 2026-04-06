#include "editor_state.hpp"

#include <lua.h>
#include <lualib.h>

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

void eng_lua_register_editor(lua_State *L)
{
    lua_newtable(L);
    lua_pushcfunction(L, l_editor_playState, "editor.playState");
    lua_setfield(L, -2, "playState");
    lua_pushcfunction(L, l_editor_getSelectedEntityId, "editor.getSelectedEntityId");
    lua_setfield(L, -2, "getSelectedEntityId");
    lua_setglobal(L, "editor");
}
