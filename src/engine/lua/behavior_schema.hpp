#pragma once

#include <nlohmann/json.hpp>

#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

struct lua_State;

/// Encodes a Lua `LUA_TNUMBER` for JSON / storage. Luau's `lua_tointegerx` out-flag means "is a
/// number", not "is an integer" — do not use it to choose float vs int.
nlohmann::json eng_behavior_lua_number_to_json(lua_State* L, int idx);

struct BehaviorPropField {
    std::string                name;
    std::string                type;
    nlohmann::json             defaultValue;
    std::optional<double>      minVal;
    std::optional<double>      maxVal;
    std::vector<std::string>   enumOptions;
};

struct BehaviorSchema {
    /// Iteration order from the schema table (Luau insertion / `lua_next` order).
    std::vector<BehaviorPropField> order;
};

void eng_behavior_schema_clear();

/// Reads `properties` from the behavior module table at `moduleStackIndex`.
void eng_behavior_schema_register_from_module(lua_State* L,
                                              const char*    behaviorName,
                                              int            moduleStackIndex);

const BehaviorSchema* eng_behavior_schema_find(const char* behaviorName);

nlohmann::json eng_behavior_merge_properties(const char*               behaviorName,
                                             const nlohmann::json&     overrides);

/// Returns only keys whose values differ from schema defaults (for scene JSON).
nlohmann::json eng_behavior_overrides_for_save(const char*           behaviorName,
                                               const nlohmann::json& propertyOverrides);

/// Returns empty string on success, otherwise an error message.
std::string eng_behavior_coerce_to_json(const BehaviorPropField& field,
                                        const nlohmann::json&    in,
                                        nlohmann::json&          out);

/// Editor / bridge: field list with defaults and metadata, or null if no schema.
nlohmann::json eng_behavior_schema_to_editor_json(const char* behaviorName);
