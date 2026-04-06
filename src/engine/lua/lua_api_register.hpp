#pragma once

struct lua_State;

/// Registers `draw`, `input`, `audio`, `screen`, `app`, `runtime`, `editor`, `require`, transform
/// metatable.
void eng_lua_register_apis(lua_State* L);
