#pragma once

#include <cstdint>

struct lua_State;

/// Userdata payload for `runtime.getTransform` (must match `__index` / `__newindex`).
struct EngTransformUdata {
    uint32_t entityId = 0;
};

void eng_lua_register_transform_metatable(lua_State* L);

/// Luau metatable name for `runtime.getTransform` userdata.
const char* eng_lua_transform_typename();
