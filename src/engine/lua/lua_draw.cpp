#include <lua.h>
#include <lualib.h>

#include <SDL3/SDL.h>
#include <vector>

#include "engine/engine.hpp"
#include "engine/immediate_draw.hpp"
#include "lua_util.hpp"

static int l_draw_clear(lua_State* L) {
    Uint8 r = (Uint8)luaL_optinteger(L, 1, 0);
    Uint8 g = (Uint8)luaL_optinteger(L, 2, 0);
    Uint8 b = (Uint8)luaL_optinteger(L, 3, 0);
    SDL_SetRenderDrawColor(g_eng.renderer, r, g, b, 255);
    SDL_FRect bg = { 0.f, 0.f, (float)s_game_lu_w, (float)s_game_lu_h };
    SDL_RenderFillRect(g_eng.renderer, &bg);
    return 0;
}

static int l_draw_line(lua_State* L) {
    float x1 = (float)luaL_checknumber(L, 1);
    float y1 = (float)luaL_checknumber(L, 2);
    float x2 = (float)luaL_checknumber(L, 3);
    float y2 = (float)luaL_checknumber(L, 4);
    SDL_SetRenderDrawColor(g_eng.renderer,
                           (Uint8)luaL_optinteger(L, 5, 255),
                           (Uint8)luaL_optinteger(L, 6, 255),
                           (Uint8)luaL_optinteger(L, 7, 255),
                           (Uint8)luaL_optinteger(L, 8, 255));
    const Affine2D& m = eng_draw_current_matrix();
    Vec2 a = m.transformPoint(x1, y1);
    Vec2 b = m.transformPoint(x2, y2);
    SDL_RenderLine(g_eng.renderer, a.x, a.y, b.x, b.y);
    return 0;
}

static int l_draw_circle(lua_State* L) {
    float cx     = (float)luaL_checknumber(L, 1);
    float cy     = (float)luaL_checknumber(L, 2);
    float radius = (float)luaL_checknumber(L, 3);
    SDL_Color col = eng_lua_color(L, 4, 5, 6, 7);
    eng_immediate_draw_circle(g_eng.renderer, { cx, cy }, radius, col);
    return 0;
}

static int l_draw_poly(lua_State* L) {
    luaL_checktype(L, 1, LUA_TTABLE);
    std::vector<Vec2> pts;
    lua_pushnil(L);
    while (lua_next(L, 1)) {
        lua_rawgeti(L, -1, 1);
        float x = (float)lua_tonumber(L, -1);
        lua_pop(L, 1);
        lua_rawgeti(L, -1, 2);
        float y = (float)lua_tonumber(L, -1);
        lua_pop(L, 1);
        pts.push_back({ x, y });
        lua_pop(L, 1);
    }
    float     px    = (float)luaL_checknumber(L, 2);
    float     py    = (float)luaL_checknumber(L, 3);
    float     angle = (float)luaL_checknumber(L, 4);
    SDL_Color col   = eng_lua_color(L, 5, 6, 7, 8);
    eng_immediate_draw_poly(g_eng.renderer, pts, { px, py }, angle, col);
    return 0;
}

static int l_draw_char(lua_State* L) {
    const char* s = luaL_checkstring(L, 1);
    float       x = (float)luaL_checknumber(L, 2);
    float       y = (float)luaL_checknumber(L, 3);
    float scale   = (float)luaL_optnumber(L, 4, 1.0);
    SDL_Color col = eng_lua_color(L, 5, 6, 7, 8);
    eng_immediate_draw_char(g_eng.renderer, s[0], x, y, scale, col);
    return 0;
}

static int l_draw_number(lua_State* L) {
    int       n     = (int)luaL_checkinteger(L, 1);
    float     x     = (float)luaL_checknumber(L, 2);
    float     y     = (float)luaL_checknumber(L, 3);
    float     scale = (float)luaL_optnumber(L, 4, 1.0);
    SDL_Color col   = eng_lua_color(L, 5, 6, 7, 8);
    eng_immediate_draw_number(g_eng.renderer, n, x, y, scale, col);
    return 0;
}

static int l_draw_present(lua_State* /*L*/) {
    SDL_RenderPresent(g_eng.renderer);
    return 0;
}

// ── Matrix stack Lua API ─────────────────────────────────────────────────────

static int l_draw_pushMatrix(lua_State* /*L*/) {
    eng_draw_push_matrix(eng_draw_current_matrix());
    return 0;
}

static int l_draw_popMatrix(lua_State* /*L*/) {
    eng_draw_pop_matrix();
    return 0;
}

static int l_draw_resetMatrix(lua_State* /*L*/) {
    eng_draw_set_matrix(Affine2D::identity());
    return 0;
}

static int l_draw_translate(lua_State* L) {
    float x = (float)luaL_checknumber(L, 1);
    float y = (float)luaL_checknumber(L, 2);
    const Affine2D& cur = eng_draw_current_matrix();
    eng_draw_set_matrix(cur.multiply(Affine2D::fromTranslation(x, y)));
    return 0;
}

static int l_draw_translateScreen(lua_State* L) {
    float x = (float)luaL_checknumber(L, 1);
    float y = (float)luaL_checknumber(L, 2);
    const Affine2D& cur = eng_draw_current_matrix();
    eng_draw_set_matrix(Affine2D::fromTranslation(x, y).multiply(cur));
    return 0;
}

static int l_draw_rotate(lua_State* L) {
    float angleDeg = (float)luaL_checknumber(L, 1);
    float rad = angleDeg * ENG_DEG2RAD;
    float ca = cosf(rad), sa = sinf(rad);
    Affine2D rot = { ca, -sa, 0, sa, ca, 0 };
    eng_draw_set_matrix(eng_draw_current_matrix().multiply(rot));
    return 0;
}

static int l_draw_scale(lua_State* L) {
    float sx = (float)luaL_checknumber(L, 1);
    float sy = (float)luaL_optnumber(L, 2, sx);
    Affine2D sc = { sx, 0, 0, 0, sy, 0 };
    eng_draw_set_matrix(eng_draw_current_matrix().multiply(sc));
    return 0;
}

void eng_lua_register_draw(lua_State* L) {
    lua_newtable(L);
    lua_pushcfunction(L, l_draw_clear, "draw.clear");
    lua_setfield(L, -2, "clear");
    lua_pushcfunction(L, l_draw_line, "draw.line");
    lua_setfield(L, -2, "line");
    lua_pushcfunction(L, l_draw_circle, "draw.circle");
    lua_setfield(L, -2, "circle");
    lua_pushcfunction(L, l_draw_poly, "draw.poly");
    lua_setfield(L, -2, "poly");
    lua_pushcfunction(L, l_draw_char, "draw.char");
    lua_setfield(L, -2, "char");
    lua_pushcfunction(L, l_draw_number, "draw.number");
    lua_setfield(L, -2, "number");
    lua_pushcfunction(L, l_draw_present, "draw.present");
    lua_setfield(L, -2, "present");
    lua_pushcfunction(L, l_draw_pushMatrix, "draw.pushMatrix");
    lua_setfield(L, -2, "pushMatrix");
    lua_pushcfunction(L, l_draw_popMatrix, "draw.popMatrix");
    lua_setfield(L, -2, "popMatrix");
    lua_pushcfunction(L, l_draw_resetMatrix, "draw.resetMatrix");
    lua_setfield(L, -2, "resetMatrix");
    lua_pushcfunction(L, l_draw_translate, "draw.translate");
    lua_setfield(L, -2, "translate");
    lua_pushcfunction(L, l_draw_translateScreen, "draw.translateScreen");
    lua_setfield(L, -2, "translateScreen");
    lua_pushcfunction(L, l_draw_rotate, "draw.rotate");
    lua_setfield(L, -2, "rotate");
    lua_pushcfunction(L, l_draw_scale, "draw.scale");
    lua_setfield(L, -2, "scale");
    lua_setglobal(L, "draw");
}
