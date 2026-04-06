#pragma once

#include <cstdint>

class Scene;

/// Monotonic id for the embedding's main behavior `lua_State`. Incremented only when that VM is
/// replaced (`lua_close` + new `eng_create_lua_vm`). Used with `ScriptInstance::scriptVmGen` so stale
/// registry refs are never used after a swap.
uint32_t eng_lua_vm_generation();

/// Call immediately after the process-wide VM pointer has been updated following a full VM
/// replacement. Bumps generation, clears all `ScriptInstance` Luau refs without calling Lua, and
/// logs at debug priority. Does not close or create VMs.
void eng_on_lua_vm_replaced(Scene& scene);
