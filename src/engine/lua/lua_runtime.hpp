#pragma once

struct lua_State;
class Scene;

/// Call `start` (once) + `update` on behavior tables for entities (sorted by update order).
void eng_scene_update_lua_scripts(lua_State* L, Scene& scene, float dt);

/// Call `draw` on behavior tables for entities (sorted by draw order).
void eng_scene_draw_lua_scripts(lua_State* L, Scene& scene, float totalTime);

/// Call `keydown(id, key)` on behavior tables for entities (sorted by update order).
void eng_scene_keydown_lua_scripts(lua_State* L, Scene& scene, const char* key);

/// Call `onHudPlay(id)` on behavior tables for entities (sorted by update order).
void eng_scene_hudplay_lua_scripts(lua_State* L, Scene& scene);

void eng_lua_register_runtime(lua_State* L);
