#pragma once

#include <nlohmann/json.hpp>

struct lua_State;
struct ScriptInstance;

/// Release registry ref for this script slot (safe if LUA_NOREF).
void eng_behavior_release_script_self(lua_State* L, ScriptInstance& sc);

void eng_behavior_push_script_self(lua_State* L, ScriptInstance& sc, uint32_t entityId);

/// Push a JSON value onto the Lua stack (for merged property bag).
void eng_behavior_push_json(lua_State* L, const nlohmann::json& j);
