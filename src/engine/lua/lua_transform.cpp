#include "lua_transform.hpp"

#include <lua.h>
#include <lualib.h>

#include <cstring>

#include "engine/engine.hpp"
#include "scene.hpp"

static const char kTransformUdataType[] = "TransformHandle";

const char* eng_lua_transform_typename() {
    return kTransformUdataType;
}

static int l_transform_index(lua_State* L) {
    auto* u         = (EngTransformUdata*)luaL_checkudata(L, 1, kTransformUdataType);
    Entity* e = g_scene.entity(u->entityId);
    if (!e)
        luaL_error(L, "Transform: entity no longer exists");
    const Transform *t = e->getTransform();
    if (!t)
        luaL_error(L, "Transform: entity has no Transform behavior");
    const char *k = luaL_checkstring(L, 2);
    if (!std::strcmp(k, "x"))
        lua_pushnumber(L, t->x);
    else if (!std::strcmp(k, "y"))
        lua_pushnumber(L, t->y);
    else if (!std::strcmp(k, "angle"))
        lua_pushnumber(L, t->angle);
    else if (!std::strcmp(k, "vx"))
        lua_pushnumber(L, t->vx);
    else if (!std::strcmp(k, "vy"))
        lua_pushnumber(L, t->vy);
    else if (!std::strcmp(k, "sx"))
        lua_pushnumber(L, t->sx);
    else if (!std::strcmp(k, "sy"))
        lua_pushnumber(L, t->sy);
    else
        luaL_error(L, "invalid Transform field");
    return 1;
}

static int l_transform_newindex(lua_State* L) {
    auto* u   = (EngTransformUdata*)luaL_checkudata(L, 1, kTransformUdataType);
    Entity* e = g_scene.entity(u->entityId);
    if (!e)
        luaL_error(L, "Transform: entity no longer exists");
    Transform *t = e->getTransform();
    if (!t)
        luaL_error(L, "Transform: entity has no Transform behavior");
    const char* k = luaL_checkstring(L, 2);
    float       v = (float)luaL_checknumber(L, 3);
    if (!std::strcmp(k, "x"))
        t->x = v;
    else if (!std::strcmp(k, "y"))
        t->y = v;
    else if (!std::strcmp(k, "angle"))
        t->angle = v;
    else if (!std::strcmp(k, "vx"))
        t->vx = v;
    else if (!std::strcmp(k, "vy"))
        t->vy = v;
    else if (!std::strcmp(k, "sx"))
        t->sx = v;
    else if (!std::strcmp(k, "sy"))
        t->sy = v;
    else
        luaL_error(L, "invalid Transform field");
    return 0;
}

void eng_lua_register_transform_metatable(lua_State* L) {
    luaL_newmetatable(L, kTransformUdataType);
    lua_pushcfunction(L, l_transform_index, "transform.__index");
    lua_setfield(L, -2, "__index");
    lua_pushcfunction(L, l_transform_newindex, "transform.__newindex");
    lua_setfield(L, -2, "__newindex");
    lua_pop(L, 1);
}
