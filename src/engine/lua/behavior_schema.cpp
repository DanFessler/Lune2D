#include "behavior_schema.hpp"

#include <SDL3/SDL.h>

#include <cmath>
#include <cstring>
#include <cstdint>
#include <lua.h>
#include <lualib.h>

static std::unordered_map<std::string, BehaviorSchema> g_behaviorSchemas;

void eng_behavior_schema_clear() {
    g_behaviorSchemas.clear();
}

nlohmann::json eng_behavior_lua_number_to_json(lua_State* L, int idx) {
    double n = lua_tonumber(L, idx);
    if (!std::isfinite(n))
        return nlohmann::json(nullptr);
    double intpart = 0;
    double frac    = std::modf(n, &intpart);
    if (frac == 0.0 && intpart >= static_cast<double>(INT64_MIN) &&
        intpart <= static_cast<double>(INT64_MAX))
        return nlohmann::json(static_cast<int64_t>(intpart));
    return nlohmann::json(n);
}

static nlohmann::json luaValueToJson(lua_State* L, int idx) {
    int t = lua_type(L, idx);
    switch (t) {
    case LUA_TNUMBER:
        return eng_behavior_lua_number_to_json(L, idx);
    case LUA_TBOOLEAN:
        return nlohmann::json(lua_toboolean(L, idx) != 0);
    case LUA_TSTRING:
        return nlohmann::json(std::string(lua_tostring(L, idx)));
    case LUA_TNIL:
        return nlohmann::json(nullptr);
    case LUA_TTABLE: {
        // Heuristic: array-like {n,...} -> json array; else object (not used for defaults v1)
        size_t       len = lua_objlen(L, idx);
        nlohmann::json arr = nlohmann::json::array();
        if (len > 0) {
            for (size_t i = 1; i <= len; ++i) {
                lua_rawgeti(L, idx, (int)i);
                arr.push_back(luaValueToJson(L, -1));
                lua_pop(L, 1);
            }
            return arr;
        }
        return nlohmann::json::object();
    }
    default:
        return nlohmann::json(nullptr);
    }
}

static void readEnumOptions(lua_State* L, int descIdx, BehaviorPropField& out) {
    lua_getfield(L, descIdx, "enumOptions");
    if (!lua_istable(L, -1)) {
        lua_pop(L, 1);
        return;
    }
    size_t n = lua_objlen(L, -1);
    for (size_t i = 1; i <= n; ++i) {
        lua_rawgeti(L, -1, (int)i);
        if (lua_isstring(L, -1))
            out.enumOptions.push_back(lua_tostring(L, -1));
        lua_pop(L, 1);
    }
    lua_pop(L, 1);
}

void eng_behavior_schema_register_from_module(lua_State* L,
                                              const char* behaviorName,
                                              int         moduleStackIndex) {
    if (!behaviorName || !behaviorName[0])
        return;

    int m = lua_absindex(L, moduleStackIndex);
    lua_getfield(L, m, "properties");
    if (lua_isnil(L, -1)) {
        lua_pop(L, 1);
        return;
    }
    if (!lua_istable(L, -1)) {
        SDL_Log("Behavior '%s': properties must be a table", behaviorName);
        lua_pop(L, 1);
        return;
    }

    lua_getfield(L, -1, "__kind");
    const char* kind = lua_tostring(L, -1);
    lua_pop(L, 1);
    if (!kind || std::strcmp(kind, "BehaviorProperties") != 0) {
        SDL_Log("Behavior '%s': properties must be created with defineProperties {...}", behaviorName);
        lua_pop(L, 1);
        return;
    }

    lua_getfield(L, -1, "fields");
    if (!lua_istable(L, -1)) {
        SDL_Log("Behavior '%s': properties.fields missing", behaviorName);
        lua_pop(L, 2);
        return;
    }

    BehaviorSchema schema;
    lua_pushnil(L);
    while (lua_next(L, -2) != 0) {
        if (!lua_isstring(L, -2) || !lua_istable(L, -1)) {
            SDL_Log("Behavior '%s': invalid properties.fields entry", behaviorName);
            lua_pop(L, 2);
            lua_pop(L, 2);
            lua_pop(L, 1);
            return;
        }
        BehaviorPropField f;
        f.name = lua_tostring(L, -2);
        int descIdx = lua_absindex(L, -1);

        lua_getfield(L, descIdx, "type");
        f.type = lua_isstring(L, -1) ? lua_tostring(L, -1) : "";
        lua_pop(L, 1);
        if (f.type.empty()) {
            SDL_Log("Behavior '%s': property '%s' missing type", behaviorName, f.name.c_str());
            lua_pop(L, 1);
            lua_pop(L, 2);
            lua_pop(L, 1);
            return;
        }

        lua_getfield(L, descIdx, "default");
        f.defaultValue = luaValueToJson(L, -1);
        lua_pop(L, 1);

        lua_getfield(L, descIdx, "min");
        if (lua_isnumber(L, -1))
            f.minVal = lua_tonumber(L, -1);
        lua_pop(L, 1);

        lua_getfield(L, descIdx, "max");
        if (lua_isnumber(L, -1))
            f.maxVal = lua_tonumber(L, -1);
        lua_pop(L, 1);

        if (f.type == "enum")
            readEnumOptions(L, descIdx, f);

        schema.order.push_back(std::move(f));
        lua_pop(L, 1);
    }

    lua_pop(L, 1); // fields
    lua_pop(L, 1); // properties

    g_behaviorSchemas[behaviorName] = std::move(schema);
}

const BehaviorSchema* eng_behavior_schema_find(const char* behaviorName) {
    if (!behaviorName)
        return nullptr;
    auto it = g_behaviorSchemas.find(behaviorName);
    return it == g_behaviorSchemas.end() ? nullptr : &it->second;
}

std::string eng_behavior_coerce_to_json(const BehaviorPropField& field,
                                        const nlohmann::json&    in,
                                        nlohmann::json&          out) {
    try {
        if (field.type == "number") {
            if (in.is_number()) {
                out = in;
                return "";
            }
            if (in.is_string()) {
                char* end          = nullptr;
                double v           = std::strtod(in.get<std::string>().c_str(), &end);
                if (end && end != in.get<std::string>().c_str()) {
                    out = v;
                    return "";
                }
            }
            return "expected number";
        }
        if (field.type == "integer") {
            double v = 0;
            if (in.is_number_integer())
                v = (double)in.get<int64_t>();
            else if (in.is_number_float())
                v = in.get<double>();
            else if (in.is_string())
                v = std::strtod(in.get<std::string>().c_str(), nullptr);
            else
                return "expected integer";
            out = nlohmann::json(static_cast<int64_t>(std::floor(v + 0.5)));
            return "";
        }
        if (field.type == "boolean") {
            if (in.is_boolean()) {
                out = in;
                return "";
            }
            if (in.is_string()) {
                const auto& s = in.get<std::string>();
                if (s == "true") {
                    out = true;
                    return "";
                }
                if (s == "false") {
                    out = false;
                    return "";
                }
            }
            return "expected boolean";
        }
        if (field.type == "string") {
            if (in.is_string()) {
                out = in;
                return "";
            }
            if (in.is_number() || in.is_boolean()) {
                out = in.dump();
                return "";
            }
            return "expected string";
        }
        if (field.type == "enum") {
            if (!in.is_string())
                return "expected string enum value";
            const auto& s = in.get<std::string>();
            for (const auto& o : field.enumOptions) {
                if (o == s) {
                    out = s;
                    return "";
                }
            }
            return "enum value not in options";
        }
        if (field.type == "object") {
            if (in.is_object()) {
                out = in;
                return "";
            }
            if (in.is_null()) {
                out = nullptr;
                return "";
            }
            return "expected object";
        }
        if (field.type == "asset" || field.type == "color" || field.type == "vector") {
            out = in;
            return "";
        }
        return "unknown property type";
    } catch (const std::exception&) {
        return "coercion failed";
    }
}

nlohmann::json eng_behavior_merge_properties(const char*           behaviorName,
                                             const nlohmann::json& overrides) {
    const BehaviorSchema* sch = eng_behavior_schema_find(behaviorName);
    nlohmann::json        merged = nlohmann::json::object();
    if (!sch)
        return merged;

    const nlohmann::json* o = overrides.is_object() ? &overrides : nullptr;

    for (const auto& f : sch->order) {
        nlohmann::json val = f.defaultValue;
        if (o && o->contains(f.name)) {
            std::string err = eng_behavior_coerce_to_json(f, (*o)[f.name], val);
            if (!err.empty()) {
                SDL_Log("Behavior '%s' property '%s': %s", behaviorName, f.name.c_str(),
                        err.c_str());
            }
        }
        merged[f.name] = val;
    }
    return merged;
}

static bool jsonRoughlyEqual(const nlohmann::json& a, const nlohmann::json& b) {
    return a == b;
}

nlohmann::json eng_behavior_schema_to_editor_json(const char* behaviorName) {
    const BehaviorSchema* sch = eng_behavior_schema_find(behaviorName);
    if (!sch)
        return nlohmann::json(nullptr);

    nlohmann::json arr = nlohmann::json::array();
    for (const auto& f : sch->order) {
        nlohmann::json row = nlohmann::json::object();
        row["name"]         = f.name;
        row["type"]         = f.type;
        row["default"]      = f.defaultValue;
        if (f.minVal.has_value())
            row["min"] = *f.minVal;
        if (f.maxVal.has_value())
            row["max"] = *f.maxVal;
        if (!f.enumOptions.empty())
            row["enumOptions"] = f.enumOptions;
        arr.push_back(std::move(row));
    }
    return arr;
}

nlohmann::json eng_behavior_overrides_for_save(const char*           behaviorName,
                                               const nlohmann::json& propertyOverrides) {
    const BehaviorSchema* sch = eng_behavior_schema_find(behaviorName);
    if (!sch)
        return nlohmann::json::object();

    nlohmann::json merged = eng_behavior_merge_properties(behaviorName, propertyOverrides);
    nlohmann::json out    = nlohmann::json::object();

    for (const auto& f : sch->order) {
        if (!merged.contains(f.name))
            continue;
        if (!jsonRoughlyEqual(merged[f.name], f.defaultValue))
            out[f.name] = merged[f.name];
    }
    return out;
}
