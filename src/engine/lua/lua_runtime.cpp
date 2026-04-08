#include "lua_runtime.hpp"

#include <algorithm>
#include <lua.h>
#include <lualib.h>

#include <SDL3/SDL.h>

#include <nlohmann/json.hpp>

#include "behavior_instance.hpp"
#include "behavior_schema.hpp"
#include "editor_state.hpp"
#include "engine/camera.hpp"
#include "engine/engine.hpp"
#include "engine/immediate_draw.hpp"
#include "engine/scene_loader.hpp"
#include "lua_transform.hpp"
#include "scene.hpp"

lua_State *g_eng_lua_vm = nullptr;

static bool s_visual_test_skip_editor_overlays = false;

void eng_set_visual_test_skip_editor_overlays(bool skip)
{
    s_visual_test_skip_editor_overlays = skip;
}

void eng_lua_bind_main_vm(lua_State *L)
{
    g_eng_lua_vm = L;
}

bool eng_scene_mut_set_behavior_property(lua_State *L, uint32_t entityId, int behaviorIndex,
                                         const char *key, bool erase,
                                         const nlohmann::json &value)
{
    Entity *e = g_scene.entity(entityId);
    if (!e || behaviorIndex < 0 || behaviorIndex >= (int)e->behaviors.size() || !key)
        return false;
    BehaviorSlot &slot = e->behaviors[behaviorIndex];
    if (slot.isNative)
    {
        return !erase && eng_behavior_slot_set_native_property(slot, key, value);
    }
    ScriptInstance &sc = slot.script;
    if (erase)
        sc.propertyOverrides.erase(key);
    else
        sc.propertyOverrides[key] = value;
    eng_behavior_release_script_self(L, sc);
    eng_camera_sync_from_camera_behavior_mutation(entityId, behaviorIndex);
    return true;
}

bool eng_scene_mut_set_script_property(lua_State *L, uint32_t entityId, int behaviorIndex,
                                       const char *key, bool erase,
                                       const nlohmann::json &value)
{
    return eng_scene_mut_set_behavior_property(L, entityId, behaviorIndex, key, erase, value);
}

static nlohmann::json luaTableToJsonObject(lua_State *L, int idx)
{
    nlohmann::json o = nlohmann::json::object();
    int t = lua_absindex(L, idx);
    lua_pushnil(L);
    while (lua_next(L, t) != 0)
    {
        if (lua_type(L, -2) == LUA_TSTRING)
        {
            const char *k = lua_tostring(L, -2);
            if (lua_isboolean(L, -1))
                o[k] = lua_toboolean(L, -1) != 0;
            else if (lua_type(L, -1) == LUA_TNUMBER)
                o[k] = eng_behavior_lua_number_to_json(L, -1);
            else if (lua_isstring(L, -1))
                o[k] = std::string(lua_tostring(L, -1));
            else if (lua_isnil(L, -1))
                o[k] = nullptr;
        }
        lua_pop(L, 1);
    }
    return o;
}

static int l_runtime_spawn(lua_State *L)
{
    const char *name = luaL_optstring(L, 1, "Entity");
    uint32_t id = g_scene.spawn(name ? name : "Entity");
    lua_pushinteger(L, (lua_Integer)id);
    return 1;
}

static int l_runtime_destroy(lua_State *L)
{
    uint32_t id = (uint32_t)luaL_checkinteger(L, 1);
    g_scene.destroy(id);
    return 0;
}

static int l_runtime_clearScene(lua_State * /*L*/)
{
    g_scene.clear();
    return 0;
}

static int l_runtime_loadScene(lua_State *L)
{
    const char *relPath = luaL_checkstring(L, 1);
    lua_getglobal(L, "package");
    lua_getfield(L, -1, "basepath");
    const char *base = lua_tostring(L, -1);
    if (!base)
        luaL_error(L, "package.basepath not set");
    std::string fullPath = std::string(base) + relPath;
    lua_pop(L, 2);

    g_scene.clear();
    if (!eng_load_scene(g_scene, fullPath))
        luaL_error(L, "loadScene failed: %s", fullPath.c_str());
    return 0;
}

static int l_runtime_addScript(lua_State *L)
{
    uint32_t id = (uint32_t)luaL_checkinteger(L, 1);
    const char *beh = luaL_checkstring(L, 2);
    nlohmann::json props = nlohmann::json::object();
    if (lua_gettop(L) >= 3 && lua_istable(L, 3))
        props = luaTableToJsonObject(L, 3);
    if (!g_scene.addScript(id, beh, props))
        luaL_error(L, "addScript failed");
    return 0;
}

static int l_runtime_setScriptProperty(lua_State *L)
{
    uint32_t eid = (uint32_t)luaL_checkinteger(L, 1);
    int si = (int)luaL_checkinteger(L, 2);
    const char *key = luaL_checkstring(L, 3);
    if (lua_gettop(L) < 4 || lua_isnil(L, 4))
    {
        if (!eng_scene_mut_set_script_property(L, eid, si, key, true, nlohmann::json()))
            luaL_error(L, "setScriptProperty: bad entity or script index");
        return 0;
    }
    nlohmann::json v = eng_behavior_lua_value_to_json(L, 4);
    if (!eng_scene_mut_set_script_property(L, eid, si, key, false, v))
        luaL_error(L, "setScriptProperty: bad entity or script index");
    return 0;
}

static int l_runtime_setBehaviorProperty(lua_State *L)
{
    uint32_t eid = (uint32_t)luaL_checkinteger(L, 1);
    int bi = (int)luaL_checkinteger(L, 2);
    const char *key = luaL_checkstring(L, 3);
    if (lua_gettop(L) < 4 || lua_isnil(L, 4))
    {
        if (!eng_scene_mut_set_behavior_property(L, eid, bi, key, true, nlohmann::json()))
            luaL_error(L, "setBehaviorProperty: bad entity or behavior index");
        return 0;
    }
    nlohmann::json v = eng_behavior_lua_value_to_json(L, 4);
    if (!eng_scene_mut_set_behavior_property(L, eid, bi, key, false, v))
        luaL_error(L, "setBehaviorProperty: bad entity, behavior index, or value");
    return 0;
}

static int l_runtime_setDrawOrder(lua_State *L)
{
    uint32_t id = (uint32_t)luaL_checkinteger(L, 1);
    int ord = (int)luaL_checkinteger(L, 2);
    g_scene.setDrawOrder(id, ord);
    return 0;
}

static int l_runtime_setUpdateOrder(lua_State *L)
{
    uint32_t id = (uint32_t)luaL_checkinteger(L, 1);
    int ord = (int)luaL_checkinteger(L, 2);
    g_scene.setUpdateOrder(id, ord);
    return 0;
}

static int l_runtime_setName(lua_State *L)
{
    uint32_t id = (uint32_t)luaL_checkinteger(L, 1);
    const char *name = luaL_checkstring(L, 2);
    g_scene.setName(id, name);
    return 0;
}

static int l_runtime_setActive(lua_State *L)
{
    uint32_t id = (uint32_t)luaL_checkinteger(L, 1);
    bool active = lua_toboolean(L, 2);
    g_scene.setActive(id, active);
    return 0;
}

static int l_runtime_removeScript(lua_State *L)
{
    uint32_t id = (uint32_t)luaL_checkinteger(L, 1);
    int idx = (int)luaL_checkinteger(L, 2);
    if (!g_scene.removeBehavior(id, idx))
        luaL_error(L, "removeScript failed");
    return 0;
}

static int l_runtime_reorderScript(lua_State *L)
{
    uint32_t id = (uint32_t)luaL_checkinteger(L, 1);
    int from = (int)luaL_checkinteger(L, 2);
    int to = (int)luaL_checkinteger(L, 3);
    if (!g_scene.reorderBehavior(id, from, to))
        luaL_error(L, "reorderScript failed");
    return 0;
}

static int l_runtime_setParent(lua_State *L)
{
    uint32_t id = (uint32_t)luaL_checkinteger(L, 1);
    uint32_t parentId = (uint32_t)luaL_checkinteger(L, 2);
    if (!g_scene.setParent(id, parentId))
        luaL_error(L, "setParent failed (invalid or cycle)");
    return 0;
}

static int l_runtime_removeParent(lua_State *L)
{
    uint32_t id = (uint32_t)luaL_checkinteger(L, 1);
    g_scene.removeParent(id);
    return 0;
}

static int l_runtime_setTransform(lua_State *L)
{
    uint32_t id = (uint32_t)luaL_checkinteger(L, 1);
    const char *field = luaL_checkstring(L, 2);
    float value = (float)luaL_checknumber(L, 3);
    g_scene.setTransformField(id, field, value);
    return 0;
}

static int l_runtime_getTransform(lua_State *L)
{
    uint32_t id = (uint32_t)luaL_checkinteger(L, 1);
    if (!g_scene.entity(id))
        luaL_argerror(L, 1, "invalid entity id");
    auto *u = (EngTransformUdata *)lua_newuserdata(L, sizeof(EngTransformUdata));
    u->entityId = id;
    luaL_getmetatable(L, eng_lua_transform_typename());
    lua_setmetatable(L, -2);
    return 1;
}

static int l_runtime_getWorldTransform(lua_State *L)
{
    uint32_t id = (uint32_t)luaL_checkinteger(L, 1);
    if (!g_scene.entity(id))
        luaL_argerror(L, 1, "invalid entity id");
    auto matrices = g_scene.computeWorldMatrices();
    auto it = matrices.find(id);
    Affine2D m = (it != matrices.end()) ? it->second : Affine2D::identity();
    float wx = m.tx;
    float wy = m.ty;
    float wAngle = atan2f(m.c, m.a) * (180.f / 3.14159265f);
    float wsx = sqrtf(m.a * m.a + m.c * m.c);
    float wsy = sqrtf(m.b * m.b + m.d * m.d);
    lua_newtable(L);
    lua_pushnumber(L, wx);
    lua_setfield(L, -2, "x");
    lua_pushnumber(L, wy);
    lua_setfield(L, -2, "y");
    lua_pushnumber(L, wAngle);
    lua_setfield(L, -2, "angle");
    lua_pushnumber(L, wsx);
    lua_setfield(L, -2, "sx");
    lua_pushnumber(L, wsy);
    lua_setfield(L, -2, "sy");
    return 1;
}

/// `(dlx, dly)` to add to local `x`/`y` so the entity's origin moves by `(wdx, wdy)` in world space.
static int l_runtime_worldDeltaToLocal(lua_State *L)
{
    uint32_t id = (uint32_t)luaL_checkinteger(L, 1);
    float wdx = (float)luaL_checknumber(L, 2);
    float wdy = (float)luaL_checknumber(L, 3);
    if (!g_scene.entity(id))
        luaL_argerror(L, 1, "invalid entity id");
    float dlx = 0.f, dly = 0.f;
    g_scene.worldDeltaToLocalPositionDelta(id, wdx, wdy, &dlx, &dly);
    lua_pushnumber(L, dlx);
    lua_pushnumber(L, dly);
    return 2;
}

static int l_runtime_registerBehavior(lua_State *L)
{
    const char *name = luaL_checkstring(L, 1);
    luaL_checktype(L, 2, LUA_TTABLE);
    eng_behavior_schema_register_from_module(L, name, 2);
    lua_getglobal(L, "_BEHAVIORS");
    lua_pushvalue(L, 2);
    lua_setfield(L, -2, name);
    lua_pop(L, 1);

    auto &list = g_registered_behaviors;
    if (std::find(list.begin(), list.end(), name) == list.end())
        list.push_back(name);

    return 0;
}

void eng_scene_draw_lua_scripts(lua_State *L, Scene &scene, float totalTime)
{
    auto &cam = eng_camera_state();
    bool usePlayCam = (eng_editor_sim_ui_state() == EngSimUiState::Playing &&
                       cam.activeCameraEntityId != 0 &&
                       scene.entity(cam.activeCameraEntityId));
    if (usePlayCam)
    {
        auto matrices = scene.computeWorldMatrices();
        auto it = matrices.find(cam.activeCameraEntityId);
        if (it != matrices.end())
        {
            const Affine2D &wm = it->second;
            float cx = wm.tx;
            float cy = wm.ty;
            float ca = atan2f(wm.c, wm.a) * (180.f / 3.14159265f);
            float vf = cam.playCamVfov > 0 ? cam.playCamVfov : (float)s_game_lu_h;
            cam.viewMatrix = eng_compute_view_matrix(cx, cy, ca, vf, s_game_lu_w, s_game_lu_h);
            cam.inverseViewMatrix = cam.viewMatrix.inverse();
        }
        else
            eng_camera_compute_view(s_game_lu_w, s_game_lu_h);
    }
    else
        eng_camera_compute_view(s_game_lu_w, s_game_lu_h);

    const Affine2D &viewMatrix = cam.viewMatrix;

    auto worldMatrices = scene.computeWorldMatrices();

    scene.forEachEntitySortedByDrawOrder([&](Entity &e)
                                         {
        if (!e.active)
            return;

        auto it = worldMatrices.find(e.id);
        Affine2D wm = (it != worldMatrices.end()) ? it->second : Affine2D::identity();
        eng_draw_push_matrix(viewMatrix.multiply(wm));

        for (BehaviorSlot& b : e.behaviors) {
            if (b.isNative) continue;
            ScriptInstance& sc = b.script;
            lua_getglobal(L, "_BEHAVIORS");
            if (lua_isnil(L, -1)) {
                lua_pop(L, 1);
                break;
            }
            lua_getfield(L, -1, sc.behavior.c_str());
            if (!lua_istable(L, -1)) {
                lua_pop(L, 2);
                continue;
            }
            lua_getfield(L, -1, "draw");
            if (lua_isfunction(L, -1)) {
                eng_behavior_push_script_self(L, sc, e.id);
                lua_pushnumber(L, totalTime);
                if (lua_pcall(L, 2, 0, 0) != LUA_OK) {
                    SDL_Log("Lua %s.draw: %s", sc.behavior.c_str(), lua_tostring(L, -1));
                    lua_pop(L, 1);
                }
            } else
                lua_pop(L, 1);
            lua_pop(L, 2);
        }

        eng_draw_pop_matrix(); });
}

static int l_runtime_forEachEntityDrawOrder(lua_State *L)
{
    luaL_checktype(L, 1, LUA_TFUNCTION);
    g_scene.forEachEntitySortedByDrawOrder([&](Entity &e) {
        if (!e.active)
            return;
        lua_pushvalue(L, 1);
        lua_pushinteger(L, (lua_Integer)e.id);
        if (lua_pcall(L, 1, 0, 0) != LUA_OK)
        {
            SDL_Log("Lua runtime.forEachEntityDrawOrder: %s", lua_tostring(L, -1));
            lua_pop(L, 1);
        }
    });
    return 0;
}

void eng_scene_draw_editor_overlays(lua_State *L, Scene &scene, float totalTime)
{
    if (s_visual_test_skip_editor_overlays)
        return;
    if (!eng_editor_overlays_enabled())
        return;

    const Affine2D &viewMatrix = eng_camera_state().viewMatrix;

    lua_getglobal(L, "_EDITOR_BEHAVIORS");
    if (!lua_istable(L, -1))
    {
        lua_pop(L, 1);
        return;
    }
    eng_draw_push_matrix(viewMatrix);

    lua_getfield(L, -1, "Transform");
    if (lua_istable(L, -1))
    {
        lua_getfield(L, -1, "drawWorld");
        lua_remove(L, -2);
        lua_remove(L, -2);
        if (lua_isfunction(L, -1))
        {
            lua_pushnumber(L, totalTime);
            if (lua_pcall(L, 1, 0, 0) != LUA_OK)
            {
                SDL_Log("Lua editor Transform.drawWorld: %s", lua_tostring(L, -1));
                lua_pop(L, 1);
            }
        }
        else
            lua_pop(L, 1);
    }
    else
        lua_pop(L, 2);

    eng_draw_pop_matrix();

    auto worldMatrices = scene.computeWorldMatrices();
    scene.forEachEntitySortedByDrawOrder([&](Entity &e) {
        if (!e.active)
            return;
        auto it = worldMatrices.find(e.id);
        Affine2D wm = (it != worldMatrices.end()) ? it->second : Affine2D::identity();
        eng_draw_push_matrix(viewMatrix.multiply(wm));
        for (BehaviorSlot &b : e.behaviors)
        {
            if (b.isNative) continue;
            ScriptInstance &sc = b.script;
            lua_getglobal(L, "_EDITOR_BEHAVIORS");
            if (lua_isnil(L, -1))
            {
                lua_pop(L, 1);
                break;
            }
            lua_getfield(L, -1, sc.behavior.c_str());
            if (!lua_istable(L, -1))
            {
                lua_pop(L, 2);
                continue;
            }
            lua_getfield(L, -1, "draw");
            if (lua_isfunction(L, -1))
            {
                eng_behavior_push_script_self(L, sc, e.id);
                lua_pushnumber(L, totalTime);
                if (lua_pcall(L, 2, 0, 0) != LUA_OK)
                {
                    SDL_Log("Lua editor %s.draw: %s", sc.behavior.c_str(), lua_tostring(L, -1));
                    lua_pop(L, 1);
                }
            }
            else
                lua_pop(L, 1);
            lua_pop(L, 2);
        }
        eng_draw_pop_matrix();
    });
}

void eng_scene_update_editor_behaviors(lua_State *L, Scene &scene, float dt)
{
    if (!eng_editor_overlays_enabled())
        return;

    lua_getglobal(L, "_EDITOR_BEHAVIORS");
    if (!lua_istable(L, -1))
    {
        lua_pop(L, 1);
        return;
    }

    // EditorCamera.updateWorld runs BEFORE Transform so the view matrix is
    // current when gizmo picking converts screen coords to world coords.
    lua_getfield(L, -1, "EditorCamera");
    if (lua_istable(L, -1))
    {
        lua_getfield(L, -1, "updateWorld");
        lua_remove(L, -2);
        if (lua_isfunction(L, -1))
        {
            lua_pushnumber(L, dt);
            if (lua_pcall(L, 1, 0, 0) != LUA_OK)
            {
                SDL_Log("Lua editor EditorCamera.updateWorld: %s", lua_tostring(L, -1));
                lua_pop(L, 1);
            }
        }
        else
            lua_pop(L, 1);
    }
    else
        lua_pop(L, 1);

    // Pop the _EDITOR_BEHAVIORS table from the initial getglobal.
    lua_pop(L, 1);

    // Recompute view matrix after EditorCamera may have changed camera state.
    eng_camera_compute_view(s_game_lu_w, s_game_lu_h);

    lua_getglobal(L, "_EDITOR_BEHAVIORS");
    if (!lua_istable(L, -1))
    {
        lua_pop(L, 1);
        return;
    }
    lua_getfield(L, -1, "Transform");
    if (lua_istable(L, -1))
    {
        lua_getfield(L, -1, "updateWorld");
        lua_remove(L, -2);
        lua_remove(L, -2);
        if (lua_isfunction(L, -1))
        {
            lua_pushnumber(L, dt);
            if (lua_pcall(L, 1, 0, 0) != LUA_OK)
            {
                SDL_Log("Lua editor Transform.updateWorld: %s", lua_tostring(L, -1));
                lua_pop(L, 1);
            }
        }
        else
            lua_pop(L, 1);
    }
    else
        lua_pop(L, 2);
}

static int l_runtime_drawBehaviors(lua_State *L)
{
    float totalTime = (float)luaL_checknumber(L, 1);
    eng_scene_draw_lua_scripts(L, g_scene, totalTime);
    return 0;
}

void eng_scene_update_lua_scripts(lua_State *L, Scene &scene, float dt)
{
    scene.forEachEntitySortedByUpdateOrder([&](Entity &e)
                                           {
        if (!e.active)
            return;
        for (BehaviorSlot& b : e.behaviors) {
            if (b.isNative) continue;
            ScriptInstance& sc = b.script;
            lua_getglobal(L, "_BEHAVIORS");
            if (lua_isnil(L, -1)) {
                lua_pop(L, 1);
                break;
            }
            lua_getfield(L, -1, sc.behavior.c_str());
            if (!lua_istable(L, -1)) {
                lua_pop(L, 2);
                continue;
            }
            if (!sc.started) {
                lua_getfield(L, -1, "start");
                if (lua_isfunction(L, -1)) {
                    eng_behavior_push_script_self(L, sc, e.id);
                    if (lua_pcall(L, 1, 0, 0) != LUA_OK) {
                        SDL_Log("Lua %s.start: %s", sc.behavior.c_str(), lua_tostring(L, -1));
                        lua_pop(L, 1);
                    }
                } else
                    lua_pop(L, 1);
                sc.started = true;
            }
            lua_getfield(L, -1, "update");
            if (lua_isfunction(L, -1)) {
                eng_behavior_push_script_self(L, sc, e.id);
                lua_pushnumber(L, dt);
                if (lua_pcall(L, 2, 0, 0) != LUA_OK) {
                    SDL_Log("Lua %s.update: %s", sc.behavior.c_str(), lua_tostring(L, -1));
                    lua_pop(L, 1);
                }
            } else
                lua_pop(L, 1);
            lua_pop(L, 2);
        } });
}

void eng_scene_keydown_lua_scripts(lua_State *L, Scene &scene, const char *key)
{
    scene.forEachEntitySortedByUpdateOrder([&](Entity &e)
                                           {
        if (!e.active)
            return;
        for (BehaviorSlot& b : e.behaviors) {
            if (b.isNative) continue;
            ScriptInstance& sc = b.script;
            lua_getglobal(L, "_BEHAVIORS");
            if (lua_isnil(L, -1)) {
                lua_pop(L, 1);
                break;
            }
            lua_getfield(L, -1, sc.behavior.c_str());
            if (!lua_istable(L, -1)) {
                lua_pop(L, 2);
                continue;
            }
            lua_getfield(L, -1, "keydown");
            if (lua_isfunction(L, -1)) {
                eng_behavior_push_script_self(L, sc, e.id);
                lua_pushstring(L, key);
                if (lua_pcall(L, 2, 0, 0) != LUA_OK) {
                    SDL_Log("Lua %s.keydown: %s", sc.behavior.c_str(), lua_tostring(L, -1));
                    lua_pop(L, 1);
                }
            } else
                lua_pop(L, 1);
            lua_pop(L, 2);
        } });
}

void eng_scene_hudplay_lua_scripts(lua_State *L, Scene &scene)
{
    scene.forEachEntitySortedByUpdateOrder([&](Entity &e)
                                           {
        if (!e.active)
            return;
        for (BehaviorSlot& b : e.behaviors) {
            if (b.isNative) continue;
            ScriptInstance& sc = b.script;
            lua_getglobal(L, "_BEHAVIORS");
            if (lua_isnil(L, -1)) {
                lua_pop(L, 1);
                break;
            }
            lua_getfield(L, -1, sc.behavior.c_str());
            if (!lua_istable(L, -1)) {
                lua_pop(L, 2);
                continue;
            }
            lua_getfield(L, -1, "onHudPlay");
            if (lua_isfunction(L, -1)) {
                eng_behavior_push_script_self(L, sc, e.id);
                if (lua_pcall(L, 1, 0, 0) != LUA_OK) {
                    SDL_Log("Lua %s.onHudPlay: %s", sc.behavior.c_str(), lua_tostring(L, -1));
                    lua_pop(L, 1);
                }
            } else
                lua_pop(L, 1);
            lua_pop(L, 2);
        } });
}

// ── Camera API ───────────────────────────────────────────────────────────────

static int l_runtime_setPPU(lua_State *L)
{
    float ppu = (float)luaL_checknumber(L, 1);
    if (ppu < 1e-6f) ppu = 1.f;
    eng_camera_state().ppu = ppu;
    return 0;
}

static int l_runtime_getPPU(lua_State *L)
{
    lua_pushnumber(L, eng_camera_state().ppu);
    return 1;
}

static int l_runtime_setActiveCamera(lua_State *L)
{
    uint32_t id = (uint32_t)luaL_checkinteger(L, 1);
    eng_camera_state().activeCameraEntityId = id;
    return 0;
}

static int l_runtime_getActiveCameraEntityId(lua_State *L)
{
    lua_pushinteger(L, (lua_Integer)eng_camera_state().activeCameraEntityId);
    return 1;
}

static int l_runtime_setCameraProperties(lua_State *L)
{
    auto &cam = eng_camera_state();
    cam.playCamVfov = (float)luaL_checknumber(L, 1);
    cam.playBgR = (uint8_t)luaL_optinteger(L, 2, 0);
    cam.playBgG = (uint8_t)luaL_optinteger(L, 3, 0);
    cam.playBgB = (uint8_t)luaL_optinteger(L, 4, 0);
    return 0;
}

static int l_runtime_setEditorCamera(lua_State *L)
{
    auto &cam = eng_camera_state();
    cam.editorX = (float)luaL_checknumber(L, 1);
    cam.editorY = (float)luaL_checknumber(L, 2);
    cam.editorAngle = (float)luaL_optnumber(L, 3, 0.0);
    cam.editorVfov = (float)luaL_checknumber(L, 4);
    return 0;
}

static int l_runtime_screenToWorld(lua_State *L)
{
    float sx = (float)luaL_checknumber(L, 1);
    float sy = (float)luaL_checknumber(L, 2);
    float wx, wy;
    eng_camera_screen_to_world(sx, sy, &wx, &wy);
    lua_pushnumber(L, wx);
    lua_pushnumber(L, wy);
    return 2;
}

static int l_runtime_worldToScreen(lua_State *L)
{
    float wx = (float)luaL_checknumber(L, 1);
    float wy = (float)luaL_checknumber(L, 2);
    float sx, sy;
    eng_camera_world_to_screen(wx, wy, &sx, &sy);
    lua_pushnumber(L, sx);
    lua_pushnumber(L, sy);
    return 2;
}

static int l_runtime_getEditorCamera(lua_State *L)
{
    auto &cam = eng_camera_state();
    lua_newtable(L);
    lua_pushnumber(L, cam.editorX);
    lua_setfield(L, -2, "x");
    lua_pushnumber(L, cam.editorY);
    lua_setfield(L, -2, "y");
    lua_pushnumber(L, cam.editorAngle);
    lua_setfield(L, -2, "angle");
    lua_pushnumber(L, cam.editorVfov);
    lua_setfield(L, -2, "vfov");
    return 1;
}

void eng_lua_register_runtime(lua_State *L)
{
    lua_newtable(L);
    lua_pushcfunction(L, l_runtime_spawn, "runtime.spawn");
    lua_setfield(L, -2, "spawn");
    lua_pushcfunction(L, l_runtime_destroy, "runtime.destroy");
    lua_setfield(L, -2, "destroy");
    lua_pushcfunction(L, l_runtime_clearScene, "runtime.clear");
    lua_setfield(L, -2, "clear");
    lua_pushcfunction(L, l_runtime_loadScene, "runtime.loadScene");
    lua_setfield(L, -2, "loadScene");
    lua_pushcfunction(L, l_runtime_addScript, "runtime.addScript");
    lua_setfield(L, -2, "addScript");
    lua_pushcfunction(L, l_runtime_setScriptProperty, "runtime.setScriptProperty");
    lua_setfield(L, -2, "setScriptProperty");
    lua_pushcfunction(L, l_runtime_setBehaviorProperty, "runtime.setBehaviorProperty");
    lua_setfield(L, -2, "setBehaviorProperty");
    lua_pushcfunction(L, l_runtime_setDrawOrder, "runtime.setDrawOrder");
    lua_setfield(L, -2, "setDrawOrder");
    lua_pushcfunction(L, l_runtime_setUpdateOrder, "runtime.setUpdateOrder");
    lua_setfield(L, -2, "setUpdateOrder");
    lua_pushcfunction(L, l_runtime_setName, "runtime.setName");
    lua_setfield(L, -2, "setName");
    lua_pushcfunction(L, l_runtime_setActive, "runtime.setActive");
    lua_setfield(L, -2, "setActive");
    lua_pushcfunction(L, l_runtime_getTransform, "runtime.getTransform");
    lua_setfield(L, -2, "getTransform");
    lua_pushcfunction(L, l_runtime_registerBehavior, "runtime.registerBehavior");
    lua_setfield(L, -2, "registerBehavior");
    lua_pushcfunction(L, l_runtime_drawBehaviors, "runtime.drawBehaviors");
    lua_setfield(L, -2, "drawBehaviors");
    lua_pushcfunction(L, l_runtime_removeScript, "runtime.removeScript");
    lua_setfield(L, -2, "removeScript");
    lua_pushcfunction(L, l_runtime_reorderScript, "runtime.reorderScript");
    lua_setfield(L, -2, "reorderScript");
    lua_pushcfunction(L, l_runtime_setParent, "runtime.setParent");
    lua_setfield(L, -2, "setParent");
    lua_pushcfunction(L, l_runtime_removeParent, "runtime.removeParent");
    lua_setfield(L, -2, "removeParent");
    lua_pushcfunction(L, l_runtime_setTransform, "runtime.setTransform");
    lua_setfield(L, -2, "setTransform");
    lua_pushcfunction(L, l_runtime_getWorldTransform, "runtime.getWorldTransform");
    lua_setfield(L, -2, "getWorldTransform");
    lua_pushcfunction(L, l_runtime_worldDeltaToLocal, "runtime.worldDeltaToLocal");
    lua_setfield(L, -2, "worldDeltaToLocal");
    lua_pushcfunction(L, l_runtime_forEachEntityDrawOrder, "runtime.forEachEntityDrawOrder");
    lua_setfield(L, -2, "forEachEntityDrawOrder");
    lua_pushcfunction(L, l_runtime_setPPU, "runtime.setPPU");
    lua_setfield(L, -2, "setPPU");
    lua_pushcfunction(L, l_runtime_getPPU, "runtime.getPPU");
    lua_setfield(L, -2, "getPPU");
    lua_pushcfunction(L, l_runtime_setActiveCamera, "runtime.setActiveCamera");
    lua_setfield(L, -2, "setActiveCamera");
    lua_pushcfunction(L, l_runtime_getActiveCameraEntityId, "runtime.getActiveCameraEntityId");
    lua_setfield(L, -2, "getActiveCameraEntityId");
    lua_pushcfunction(L, l_runtime_setCameraProperties, "runtime.setCameraProperties");
    lua_setfield(L, -2, "setCameraProperties");
    lua_pushcfunction(L, l_runtime_setEditorCamera, "runtime.setEditorCamera");
    lua_setfield(L, -2, "setEditorCamera");
    lua_pushcfunction(L, l_runtime_getEditorCamera, "runtime.getEditorCamera");
    lua_setfield(L, -2, "getEditorCamera");
    lua_pushcfunction(L, l_runtime_screenToWorld, "runtime.screenToWorld");
    lua_setfield(L, -2, "screenToWorld");
    lua_pushcfunction(L, l_runtime_worldToScreen, "runtime.worldToScreen");
    lua_setfield(L, -2, "worldToScreen");
    lua_setglobal(L, "runtime");
}
