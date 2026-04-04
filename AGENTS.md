# AGENTS.md — ctx-style engine + editor (Asteroids sample)

This file orients AI agents and contributors to **what this repository is**, **what we’re building toward**, and **how it relates to ctx-engine / ctx-game**.

## What this project is

A **general-purpose, small game engine and editor harness**, not “an Asteroids codebase” first:

- **Native host** — **SDL3** window/render loop, input snapshot, audio, **embedded Luau** VM, `**Scene`** (entities, hierarchy, transforms, script list), immediate-mode **draw** / **screen** bindings, **runtime** scene ops from Luau.
- **Tooling shell** — **WebKit** overlay (or **Vite** in dev) with React: **scene hierarchy**, **inspector**, **Luau editor** (Monaco), toolbar **Play / Pause / Stop** (VM + editor snapshot behavior), **macOS** File menu (Save Scene ⌘S, Save As ⇧⌘S, Quit ⌘Q).

The **runtime game** lives in the SDL viewport; **editor UI** is `web/`. Native ↔ web IPC carries script/workspace ops, scene edits, and entity snapshots.

**Asteroids** is the **default example project** shipped under `lua/` (`game/`, `behaviors/`, `scenes/default.json`, etc.)—useful as a stress test and reference, but **engine changes should stay general**; don’t bake Asteroids-only assumptions into `src/engine/` when a neutral API will do.

## Goals

1. **Tight edit-run loop** — Change Luau or scene JSON, reload behaviors or full VM where appropriate, without unnecessary friction.
2. **Scene as data** — JSON under `lua/scenes/` (optional stable entity `id` for round-trip save); behaviors under `lua/behaviors/`. Other titles can add their own scenes and retire/replace the sample.
3. **Behavior model** — Each behavior module **returns a table** of lifecycle hooks (`start`, `update`, `draw`, `keydown`, `onHudPlay`, …) in `_BEHAVIORS`; C++ dispatches by name. **Strict Luau** (`--!strict`) is the norm for project scripts.
4. **Editor parity with “ctx-game”-style UX** — Dockable panels, inspector patterns, dnd-kit sortable rows where noted; parity is **behavioral**, not identical widgets.
5. **Stability** — Web: Vitest for bridge/UI. Native: CMake build green after C++/ObjC++ changes.

## Repository layout (high level)


| Area       | Role                                                                                                                                                     |
| ---------- | -------------------------------------------------------------------------------------------------------------------------------------------------------- |
| `src/`     | Engine + app glue: SDL loop, `Scene`, Luau runtime APIs, draw/input/audio, **WebKit host** (`webview_host_mac.mm`), scene load/save (`scene_loader.cpp`) |
| `lua/`     | **Example project + content** (sample Asteroids): `game/`, `behaviors/`, `scenes/`, `entities.lua`, …—treat as **content**, not the engine definition    |
| `web/`     | React **editor**: dockable layout, hierarchy, inspector, Lua panel, generated bridge helpers                                                             |
| `scripts/` | e.g. `gen-scene-ops.mjs` for RPC surface                                                                                                                 |
| `shared/`  | Cross-cutting artifacts (schema/codegen, etc.)                                                                                                           |


## Reference: **ctx-engine**

**ctx-engine** is the **architecture north star**: a compact **host** (window, frame loop, input, audio) plus **embedded Luau** and a **clear split** between engine primitives and **game/behavior scripts**. Prefer that split when extending **native** APIs vs **Luau** surface.

## Reference: **ctx-game** (UI / tooling)

**ctx-game** informs **editor UX** (dockable tools, inspector). This repo uses **@danfessler/react-dockable** and may diverge where the library does.

## Conventions for agents

- **Scope** — Minimal, task-focused diffs; no drive-by refactors or extra markdown unless asked.
- **Web** — After TS/React changes, `npm test` in `web/` when practical.
- **Native** — After C++/mm changes, `cmake --build`.
- **Lua** — Preserve `--!strict` and `require` layout under `lua/` for the **current sample**; new games can add parallel trees if needed.

## Further pointers

- Default sample scene: `lua/scenes/default.json` (legacy rows may omit `id`; saves emit `id` for round-trip).
- Bridge: `src/webview_host_mac.mm` (macOS), stub elsewhere.
- Behaviors: `src/engine/lua/script_host.`*, web `reloadBehaviors` / `restartGame`.

---

*Update when goals or sample-vs-engine boundaries shift.*