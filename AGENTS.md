# AGENTS.md — Lune2D (ctx-style engine + editor, Asteroids sample)

This file orients AI agents and contributors to **what this repository is**, **what we’re building toward**, and **how it relates to ctx-engine / [ctx-game](https://github.com/DanFessler/ctx-game/tree/develop)**. (we point to the develop branch for latest updates)

## What this project is

A **general-purpose, small game engine and editor harness**, not “an Asteroids codebase” first:

- **Native host** — **SDL3** window/render loop, input snapshot, audio, **embedded Luau** VM, **Scene** (entities, hierarchy, transforms, script list), immediate-mode **draw** / **screen** bindings, **runtime** scene ops from Luau.
- **Tooling shell** — **WebKit** overlay (or **Vite** in dev) with React: **scene hierarchy**, **inspector**, **Luau editor** (Monaco), toolbar **Play / Pause / Stop** (VM + editor snapshot behavior), **macOS** File menu (Save Scene ⌘S, Save As ⇧⌘S, Quit ⌘Q).

The **runtime game** lives in the SDL viewport; **editor UI** is `web/`. Native ↔ web IPC carries script/workspace ops, scene edits, and entity snapshots.

**Asteroids** is the **default example project** under `default-project/` (`game/`, `behaviors/`, `scenes/default.scene.json`, …). **`lua/`** holds **engine-bundled** Luau: **`lua/editor/`** (editor-only hooks in `_EDITOR_BEHAVIORS`, e.g. Transform gizmo) and **`lua/behaviors/`** (engine-provided game behaviors loaded into `_BEHAVIORS` before the project’s `behaviors/`). A project can **override** an engine behavior by defining a module with the same stem in its own `behaviors/` directory. Native-only slots (e.g. Transform) stay in C++; everything else is Luau in those folders.

## Goals

1. **Tight edit-run loop** — Change Luau or scene JSON, reload behaviors or full VM where appropriate, without unnecessary friction.
2. **Scene as data** — JSON under the loaded project’s `scenes/` (shipped sample: `default-project/scenes/`, optional stable entity `id` for round-trip save); behaviors under that project’s `behaviors/`. Other titles point the host at another project root and replace the sample.
3. **Behavior model** — Each behavior module **returns a table** of lifecycle hooks (`start`, `update`, `draw`, `keydown`, `onHudPlay`, …) in `_BEHAVIORS`; C++ dispatches by name. **Strict Luau** (`--!strict`) is the norm for project scripts.
4. **Editor parity with [ctx-game](https://github.com/DanFessler/ctx-game)-style UX** — Dockable panels, inspector patterns, dnd-kit sortable rows where noted; parity is **behavioral**, not identical widgets.
5. **Stability** — Web: Vitest for bridge/UI. Native: CMake build green after C++/ObjC++ changes.

## Repository layout (high level)

| Area               | Role                                                                                                                                                     |
| ------------------ | -------------------------------------------------------------------------------------------------------------------------------------------------------- |
| `src/`             | Engine + app glue: SDL loop, `Scene`, Luau runtime APIs, draw/input/audio, **WebKit host** (`webview_host_mac.mm`), scene load/save (`scene_loader.cpp`) |
| `lua/`             | **Engine Luau**: `behaviors/` (game behaviors for every project), `editor/` (inspector / gizmo overlays). Not the sample game’s content.                  |
| `default-project/` | **Shipped sample game** (Asteroids): `behaviors/`, `game/`, `scenes/`, `entities.lua`—stand-in for a user project directory outside the engine later.    |
| `web/`             | React **editor**: dockable layout, hierarchy, inspector, Lua panel, generated bridge helpers                                                             |
| `scripts/`         | e.g. `gen-scene-ops.mjs` for RPC surface                                                                                                                 |
| `shared/`          | Cross-cutting artifacts (schema/codegen, etc.)                                                                                                           |

## Reference: **ctx-engine**

**ctx-engine** is the **architecture north star**: a compact **host** (window, frame loop, input, audio) plus **embedded Luau** and a **clear split** between engine primitives and **game/behavior scripts**. Prefer that split when extending **native** APIs vs **Luau** surface.

## Reference: **ctx-game** (UI / tooling)

**[ctx-game](https://github.com/DanFessler/ctx-game)** — canvas2D engine with a Unity-like editor (TypeScript); informs **editor UX** here (dockable tools, inspector). This repo uses **@danfessler/react-dockable** and may diverge where the library does.

## Conventions for agents

- **Scope** — Minimal, task-focused diffs; no drive-by refactors or extra markdown unless asked.
- **Web** — After TS/React changes, `npm test` in `web/` when practical.
- **Native** — After C++/mm changes, `cmake --build`.
- **Lua** — `--!strict` for all shipped Luau. `require("editor/…")` resolves under `lua/`; `behaviors/`, `game/`, and root modules like `entities` resolve under the **project** directory (`default-project/` in-tree). Native sets `package.basepath` (project) and `package.enginepath` (`lua/`). Engine game behaviors under `lua/behaviors/` load into `_BEHAVIORS` before the project’s `behaviors/` (override by name).

## Hard bugs and incident reports

When a bug was **non-obvious** (wrong leads, cross-layer confusion, misleading errors, high debugging cost), document it:

- **Full narrative** — New file under `docs/incidents/` from `docs/incidents/TEMPLATE.md` (see `docs/incidents/README.md`).
- **Durable one-liners for agents** — Add sparingly to `docs/agent-pitfalls.md` with a link to the incident.
- **Refactor follow-up** — If structure should change, add an ordered **plan** under `docs/plans/` and link it from the incident; treat implementation as separate work unless explicitly requested.

Workflow for agents: `.cursor/skills/incident-report/SKILL.md`.

## Native host / Luau VM lifetime

- **`ScriptInstance.luaInstanceRef`** (and **`scriptVmGen`**) are valid only for the **current** main behavior VM. A scene **value copy** (snapshot, assignment) duplicates integers, not the registry they came from.
- After a **full VM replacement** (`lua_close` + new `lua_State` for behaviors), call **`eng_on_lua_vm_replaced(g_scene)`** once the global VM pointer is updated and before running behavior dispatch, or ensure all behavior refs are cleared. Same-VM **hot reload** uses **`eng_reload_behaviors`** / `releaseAllScriptLuaRefs` instead (no generation bump).

## Further pointers

- Default sample scene: `default-project/scenes/default.scene.json` (legacy rows may omit `id`; saves emit `id` for round-trip).
- Bridge: `src/webview_host_mac.mm` (macOS), stub elsewhere.
- Behaviors: `src/engine/lua/script_host.`\*, web `reloadBehaviors` / `restartGame`.

---

_Update when goals or sample-vs-engine boundaries shift._
