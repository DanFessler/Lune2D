#pragma once

#include <nlohmann/json.hpp>

struct lua_State;
class Scene;

/// Native VM used to release behavior instance refs from scene mutations (editor + Luau).
extern lua_State *g_eng_lua_vm;
void eng_lua_bind_main_vm(lua_State *L);

/// When true, `eng_scene_draw_editor_overlays` is a no-op (pixel-probe / headless visual harness).
void eng_set_visual_test_skip_editor_overlays(bool skip);

bool eng_scene_mut_set_script_property(lua_State *L, uint32_t entityId, int scriptIndex,
                                       const char *key, bool erase, const nlohmann::json &value);

bool eng_scene_mut_set_behavior_property(lua_State *L, uint32_t entityId, int behaviorIndex,
                                         const char *key, bool erase, const nlohmann::json &value);

/// Call `start` (once) + `update` on behavior tables for entities (sorted by update order).
void eng_scene_update_lua_scripts(lua_State *L, Scene &scene, float dt);

/// Call `draw` on behavior tables for entities (sorted by draw order).
void eng_scene_draw_lua_scripts(lua_State *L, Scene &scene, float totalTime);

/// Editor overlays: `_EDITOR_BEHAVIORS.Transform.drawWorld` plus per-script `_EDITOR_BEHAVIORS[name].draw`.
/// Skipped while toolbar sim state is `playing`.
void eng_scene_draw_editor_overlays(lua_State *L, Scene &scene, float totalTime);

/// Call `updateEditor` on `_EDITOR_BEHAVIORS` (Transform.updateWorld, per-script updateEditor).
/// Skipped while sim state is `playing`.
void eng_scene_update_editor_behaviors(lua_State *L, Scene &scene, float dt);

/// Call `keydown(id, key)` on behavior tables for entities (sorted by update order).
void eng_scene_keydown_lua_scripts(lua_State *L, Scene &scene, const char *key);

/// Call `onHudPlay(id)` on behavior tables for entities (sorted by update order).
void eng_scene_hudplay_lua_scripts(lua_State *L, Scene &scene);

void eng_lua_register_runtime(lua_State *L);
