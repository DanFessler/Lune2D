#include "lua_editor_input.hpp"
#include "editor_state.hpp"

#include <vector>
#include <functional>

static EditorInputState s_state;
static uint32_t s_prev_held = 0;

struct EditorPointerCallback {
    int luaRef;
};
static std::vector<EditorPointerCallback> s_on_down, s_on_move, s_on_up;

void eng_editor_input_reset()
{
    s_state = {};
    s_prev_held = 0;
}

void eng_editor_input_set_mouse(float x, float y)
{
    s_state.mouseX = x;
    s_state.mouseY = y;
}

void eng_editor_input_button_event(int button, bool down)
{
    uint32_t mask = 1u << button;
    if (down)
        s_state.buttonsHeld |= mask;
    else
        s_state.buttonsHeld &= ~mask;
}

void eng_editor_input_scroll_event(float dy)
{
    s_state.scrollDelta += dy;
}

void eng_editor_input_sync_frame_edges()
{
    s_state.buttonsPressed = s_state.buttonsHeld & ~s_prev_held;
    s_state.buttonsReleased = ~s_state.buttonsHeld & s_prev_held;
}

void eng_editor_input_end_frame()
{
    s_prev_held = s_state.buttonsHeld;
    s_state.scrollDelta = 0;
}

EditorInputState eng_editor_input_state()
{
    return s_state;
}

bool eng_editor_input_active()
{
    return eng_editor_overlays_enabled();
}

// ── Lua bindings ──

#ifdef LUA_H
// Only compile Lua bindings when Lua headers are available (not in test builds).
#endif

struct lua_State;

#if __has_include(<lua.h>)
#include <lua.h>
#include <lualib.h>

static int l_editorInput_mousePosition(lua_State *L)
{
    lua_newtable(L);
    lua_pushnumber(L, s_state.mouseX);
    lua_setfield(L, -2, "x");
    lua_pushnumber(L, s_state.mouseY);
    lua_setfield(L, -2, "y");
    return 1;
}

static int l_editorInput_mouseDown(lua_State *L)
{
    int button = (int)luaL_checkinteger(L, 1);
    uint32_t mask = 1u << button;
    lua_pushboolean(L, (s_state.buttonsHeld & mask) != 0);
    return 1;
}

static int l_editorInput_mousePressed(lua_State *L)
{
    int button = (int)luaL_checkinteger(L, 1);
    uint32_t mask = 1u << button;
    lua_pushboolean(L, (s_state.buttonsPressed & mask) != 0);
    return 1;
}

static int l_editorInput_mouseReleased(lua_State *L)
{
    int button = (int)luaL_checkinteger(L, 1);
    uint32_t mask = 1u << button;
    lua_pushboolean(L, (s_state.buttonsReleased & mask) != 0);
    return 1;
}

static int l_editorInput_scrollDelta(lua_State *L)
{
    float d = s_state.scrollDelta;
    s_state.scrollDelta = 0;
    lua_pushnumber(L, d);
    return 1;
}

void eng_lua_register_editor_input(lua_State *L)
{
    lua_newtable(L);
    lua_pushcfunction(L, l_editorInput_mousePosition, "editorInput.mousePosition");
    lua_setfield(L, -2, "mousePosition");
    lua_pushcfunction(L, l_editorInput_mouseDown, "editorInput.mouseDown");
    lua_setfield(L, -2, "mouseDown");
    lua_pushcfunction(L, l_editorInput_mousePressed, "editorInput.mousePressed");
    lua_setfield(L, -2, "mousePressed");
    lua_pushcfunction(L, l_editorInput_mouseReleased, "editorInput.mouseReleased");
    lua_setfield(L, -2, "mouseReleased");
    lua_pushcfunction(L, l_editorInput_scrollDelta, "editorInput.scrollDelta");
    lua_setfield(L, -2, "scrollDelta");
    lua_setglobal(L, "editorInput");
}

#else

void eng_lua_register_editor_input(lua_State *) {}

#endif
