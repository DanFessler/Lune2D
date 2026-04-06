#include "behavior_instance.hpp"

#include "behavior_schema.hpp"
#include "lua_vm_lifecycle.hpp"
#include "scene.hpp"

#include <lua.h>
#include <lualib.h>

void eng_behavior_push_json(lua_State* L, const nlohmann::json& j) {
    if (j.is_null()) {
        lua_pushnil(L);
        return;
    }
    if (j.is_boolean()) {
        lua_pushboolean(L, j.get<bool>() ? 1 : 0);
        return;
    }
    if (j.is_number_integer()) {
        lua_pushinteger(L, (lua_Integer)j.get<int64_t>());
        return;
    }
    if (j.is_number_float()) {
        lua_pushnumber(L, j.get<double>());
        return;
    }
    if (j.is_string()) {
        lua_pushstring(L, j.get<std::string>().c_str());
        return;
    }
    if (j.is_array()) {
        lua_newtable(L);
        int i = 1;
        for (const auto& el : j) {
            eng_behavior_push_json(L, el);
            lua_rawseti(L, -2, i);
            ++i;
        }
        return;
    }
    if (j.is_object()) {
        lua_newtable(L);
        for (auto it = j.begin(); it != j.end(); ++it) {
            lua_pushstring(L, it.key().c_str());
            eng_behavior_push_json(L, it.value());
            lua_rawset(L, -3);
        }
        return;
    }
    lua_pushnil(L);
}

void eng_behavior_release_script_self(lua_State* L, ScriptInstance& sc) {
    if (!L || sc.luaInstanceRef < 0)
        return;
    lua_unref(L, sc.luaInstanceRef);
    sc.luaInstanceRef = -1;
    sc.scriptVmGen    = 0;
}

static void fill_self_table(lua_State* L,
                            int                     tableIdx,
                            const char*             behaviorName,
                            uint32_t                entityId,
                            const nlohmann::json& merged) {
    int t = lua_absindex(L, tableIdx);

    const BehaviorSchema* sch = eng_behavior_schema_find(behaviorName);
    if (sch) {
        for (const auto& f : sch->order) {
            if (!merged.contains(f.name))
                continue;
            lua_pushstring(L, f.name.c_str());
            eng_behavior_push_json(L, merged[f.name]);
            lua_rawset(L, t);
        }
    }

    lua_pushinteger(L, (lua_Integer)entityId);
    lua_setfield(L, t, "entityId");
}

void eng_behavior_push_script_self(lua_State* L, ScriptInstance& sc, uint32_t entityId) {
    nlohmann::json merged = eng_behavior_merge_properties(sc.behavior.c_str(), sc.propertyOverrides);

    if (sc.luaInstanceRef >= 0) {
        if (sc.scriptVmGen != eng_lua_vm_generation()) {
            // Stale registry index from a prior VM — do not lua_unref/getref on this state.
            sc.luaInstanceRef = -1;
            sc.scriptVmGen    = 0;
        }
    }

    if (sc.luaInstanceRef >= 0) {
        lua_getref(L, sc.luaInstanceRef);
        if (!lua_istable(L, -1)) {
            lua_pop(L, 1);
            lua_unref(L, sc.luaInstanceRef);
            sc.luaInstanceRef = -1;
            sc.scriptVmGen    = 0;
        } else {
            fill_self_table(L, -1, sc.behavior.c_str(), entityId, merged);
            return;
        }
    }

    lua_newtable(L);
    fill_self_table(L, -1, sc.behavior.c_str(), entityId, merged);
    sc.luaInstanceRef = lua_ref(L, -1);
    sc.scriptVmGen    = eng_lua_vm_generation();
    lua_pop(L, 1);
    lua_getref(L, sc.luaInstanceRef);
}
