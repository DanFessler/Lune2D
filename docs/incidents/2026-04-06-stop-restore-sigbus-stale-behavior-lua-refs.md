# Incident: SIGBUS when pressing Stop after Play (stale behavior Lua refs)

**Date:** 2026-04-06  
**Severity / impact:** User-visible crash (`Bus error: 10` / SIGBUS) during normal editor workflow (Play → Stop).  
**Status:** resolved  
**Related:** `Scene::invalidateAllBehaviorLuaRefs`, `main.cpp` VM restart path; [plan: scene + VM ref lifecycle](../plans/2026-04-06-scene-vm-lua-ref-lifecycle.md)

## Summary

Pressing **Stop** in the toolbar triggers a full Luau VM teardown and replacement, then restores the scene from a snapshot taken when Play started. `ScriptInstance` carries `luaInstanceRef` values (Luau registry indices for pinned behavior `self` tables). Those indices are only valid for the **closed** VM. After assigning the snapshot to `g_scene`, the next frame called `lua_getref(newL, staleRef)` with indices from the old registry, which is undefined behavior and often manifested as SIGBUS on darwin/arm64.

## Timeline (optional)

- Repro: `make dev` → Play → Stop → process exits with `Bus error: 10`.

## What happened

1. **Stop** calls `reloadScripts()` → native `restartGame` → `s_script_reload_requested`.
2. Main loop: `lua_close(L)` destroys the VM and its registry.
3. `g_scene = s_scene_snapshot_before_play` copies entities **including** `ScriptInstance.luaInstanceRef` integers from the snapshot (which were valid only for the pre-close VM).
4. `create_lua_vm_only()` creates a fresh `L`; `eng_scene_update_lua_scripts` / `eng_behavior_push_script_self` eventually runs `lua_getref(L, sc.luaInstanceRef)` with garbage indices.

## Why it was hard to solve

- **Symptom looked like WebKit or dev shell instability** (`make dev`, overlay, Vite) rather than a deterministic native bug.
- **Crash type** (bus error) suggests misaligned or invalid pointer use, not an obvious Lua error string.
- **Cross-layer**: toolbar → TS bridge → native VM lifecycle + **Scene** ownership of Lua-specific state without an explicit invalidation contract after VM swap.

## Root cause

`ScriptInstance.luaInstanceRef` is VM-scoped. A **scene snapshot** is a C++ value copy: it duplicates those integers but not the VM they belong to. After `lua_close`, any non-`-1` ref is invalid. Restoring the snapshot without resetting refs reused stale registry slots on a new VM.

## Resolution

- Added `Scene::invalidateAllBehaviorLuaRefs()` to set every script’s `luaInstanceRef` to `-1` without calling Lua (safe after `lua_close`).
- Called it immediately after `g_scene = s_scene_snapshot_before_play` and `resetScriptStartedFlags()` in the `restartGame` / VM-replace path in `main.cpp`.
- Fresh `self` tables are recreated on demand by `eng_behavior_push_script_self` on the new VM.

## Mitigations for the future

- [ ] Consider a **debug assertion** in debug builds: if `lua_close` runs, assert next use of scene never calls `lua_getref` until refs are cleared (hard to centralize without a formal VM handle type).
- [ ] Add a **short comment** at `ScriptInstance::luaInstanceRef` and at snapshot assignment sites: “Invalidate after VM replacement.”
- [ ] Optional **integration test** (future harness): simulate VM swap + snapshot restore and run one tick of script dispatch without crashing.

## Agent / contributor documentation

Any code path that **closes or replaces** the main `lua_State*` while keeping `g_scene` (or a copy) must **clear or revalidate** `luaInstanceRef` (or equivalent) before the new VM runs behaviors. Snapshots are not Lua-agnostic until refs are cleared.

## Refactor / architecture follow-up

Structural hardening (VM generation or single invalidation hook, docs, logging) is captured as a **plan only** in [Scene + VM Lua ref lifecycle](../plans/2026-04-06-scene-vm-lua-ref-lifecycle.md).
