# Plan: Scene + Luau VM coupling (luaInstanceRef lifecycle)

**Status:** implemented (VM generation + `eng_on_lua_vm_replaced`, generation check in `eng_behavior_push_script_self`)  
**Triggered by:** [Stop / SIGBUS stale behavior Lua refs](../incidents/2026-04-06-stop-restore-sigbus-stale-behavior-lua-refs.md)

## Goal

Make “scene + VM” coupling explicit so future VM resets cannot forget ref invalidation.

## Non-goals

- Rewriting the entire editor bridge or snapshot format in this pass.
- Full RAII wrappers for every `lua_ref` in the codebase.

## Proposed shape

- Treat `luaInstanceRef` as tied to a **VM generation** or opaque `LuauSession*`; refuse `lua_getref` if generation mismatches.
- Alternatively: **always** `invalidateAllBehaviorLuaRefs` from a single `eng_on_lua_vm_replaced(Scene&)` hook called from exactly one place in `main.cpp`.

## Plan (ordered steps)

1. Introduce `eng_lua_vm_generation` (monotonic counter) incremented whenever `create_lua_vm_only` replaces `L`; store `uint32_t scriptVmGen` on `ScriptInstance` (or entity batch) at ref creation; `push_script_self` checks match.
2. On any VM replace, increment generation and call existing `invalidateAllBehaviorLuaRefs` (or clear gen only—refs already invalid).
3. Add a one-line log when invalidating after restore: `SDL_Log` at debug level for easier future triage.
4. Document the rule in `AGENTS.md` under native/Lua boundaries.

## Risks and regression checks

- **Risk:** Forgotten call site that replaces `L` without bumping gen or invalidating.
- **Check:** `rg lua_close` / `create_lua_vm` in repo; ensure single owner path or each path calls the hook.
- **Manual:** Play → Stop → Play → verify no crash and behaviors `start` again.

## Rollout

Single merge; no feature flag. Behavior is strictly safer.

## ADR note (optional)

If VM/session ownership grows, consider an ADR: “Scene data vs Luau session lifetime.”
