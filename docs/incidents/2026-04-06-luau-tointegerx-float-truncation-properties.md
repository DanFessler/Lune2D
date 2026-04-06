# Incident: Behavior property floats truncated (0.18 → 0, 1.1 → 1) in inspector and runtime

**Date:** 2026-04-06  
**Severity / impact:** Wrong defaults and gameplay for fractional `prop.number` / schema defaults (e.g. ship shoot cooldown, bullet life); inspector showed integers and logic used truncated values.  
**Status:** resolved  
**Related:** `eng_behavior_lua_number_to_json`, `behavior_schema.cpp`, `lua_runtime.cpp`

## Summary

Numeric behavior defaults like **0.18** and **1.1** were stored and merged as **0** and **1**. The C++ bridge used Luau’s `lua_tointegerx(L, idx, &flag)` and treated `flag` as “value is an integer.” In Luau, that flag means **“value is numeric / conversion applied”** (`isnum`), not “integral.” `lua_tointegerx` **truncates** any number toward zero, so all floats were incorrectly serialized to JSON as truncated integers.

## What happened

- `eng_behavior_schema_register_from_module` reads Lua property defaults via `luaValueToJson`.
- `luaValueToJson` used `lua_tointegerx` + `if (isint)` to choose between `int64_t` JSON and `double` JSON.
- For **every** Lua number, `isint` was true when the value was convertible, so **0.18** became **0**, **1.1** became **1** in `defaultValue` and in merged `propertyValues` sent to the web inspector.
- The same anti-pattern appeared in `luaTableToJsonObject` (`runtime.addScript` table args) and `l_runtime_setScriptProperty`.

## Why it was hard to solve

- **Looked like a UI rounding bug** first; user confirmed **runtime behavior** matched wrong values, pointing to native/merge, not React formatting.
- **Pitfall is API naming / semantics:** Luau documents `lua_tointegerx`’s third parameter as success of conversion, not integral-ness; easy to misread as “is integer.”
- **Silent data loss:** no crash—only wrong physics/timing until inspected closely.

## Root cause

Misuse of `lua_tointegerx`’s out-parameter as an “integer vs float” discriminator. Luau sets it for any successful numeric conversion and returns a **truncated** integer.

## Resolution

- Added `eng_behavior_lua_number_to_json(lua_State* L, int idx)` in `behavior_schema.cpp`: use `lua_tonumber`, then store **`int64_t` only when `std::modf` shows no fractional part** (and value fits); otherwise store **double**.
- Wired `luaValueToJson`, `luaTableToJsonObject`, and `l_runtime_setScriptProperty` through this helper.

## Mitigations for the future

- [ ] **Code search policy:** Before adding `lua_tointegerx`, read Luau `lapi.cpp` or docs; never use its out-flag to mean “is integer.”
- [ ] Optional: **clang-tidy / custom grep** in CI for `lua_tointegerx` with nearby `isint` variable name (brittle but catches copy-paste).
- [ ] Unit test (native): feed known Lua chunk with `0.18` default and assert JSON double in schema (if/when VM-in-tests exists).

## Agent / contributor documentation

For **any** Lua stack number → JSON or C++ number path in this repo, **do not** use `lua_tointegerx`’s third argument to decide float vs int. Use `lua_tonumber` + fractional test, or `eng_behavior_lua_number_to_json`.

## Refactor / architecture follow-up

_No structural refactor required._ The fix is centralized in one helper; optional follow-up is only broader test coverage for schema round-trip.
