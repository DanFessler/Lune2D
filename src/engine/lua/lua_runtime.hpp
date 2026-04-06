#pragma once

#include <nlohmann/json.hpp>

struct lua_State;
class Scene;

/// Native VM used to release behavior instance refs from scene mutations (editor + Luau).
extern lua_State* g_eng_lua_vm;
void                eng_lua_bind_main_vm(lua_State* L);

bool eng_scene_mut_set_script_property(lua_State* L, uint32_t entityId, int scriptIndex,
                                       const char* key, bool erase, const nlohmann::json& value);

/// Call `start` (once) + `update` on behavior tables for entities (sorted by update order).
void eng_scene_update_lua_scripts(lua_State* L, Scene& scene, float dt);

/// Call `draw` on behavior tables for entities (sorted by draw order).
void eng_scene_draw_lua_scripts(lua_State* L, Scene& scene, float totalTime);

/// Call `keydown(id, key)` on behavior tables for entities (sorted by update order).
void eng_scene_keydown_lua_scripts(lua_State* L, Scene& scene, const char* key);

/// Call `onHudPlay(id)` on behavior tables for entities (sorted by update order).
void eng_scene_hudplay_lua_scripts(lua_State* L, Scene& scene);

void eng_lua_register_runtime(lua_State* L);
