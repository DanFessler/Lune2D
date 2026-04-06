# Agent pitfalls (durable lessons)

Short reminders for AI agents and contributors. Each item should **link** to a full write-up in `docs/incidents/` when possible.

_Add bullets only after an incident or review—not speculative noise._

---

- **Luau VM replace + scene snapshot** — After `lua_close` / VM swap, `ScriptInstance.luaInstanceRef` must be cleared (`invalidateAllBehaviorLuaRefs`) before the new VM runs behaviors; snapshot copy duplicates stale registry IDs. See [Stop / SIGBUS stale behavior refs](incidents/2026-04-06-stop-restore-sigbus-stale-behavior-lua-refs.md).

- **`lua_tointegerx` is not "is this an integer?"** — The third out-flag means "numeric conversion succeeded"; the returned value is truncated. For float-preserving JSON (behavior defaults, `addScript` tables), use `eng_behavior_lua_number_to_json` or `lua_tonumber` + a fractional check. See [Float truncation in properties](incidents/2026-04-06-luau-tointegerx-float-truncation-properties.md).

- **Never override `hitTest:withEvent:` on a WKWebView subclass.** It is not a public `NSView` method; WKWebView does not implement it. `[super hitTest:point withEvent:event]` crashes with "unrecognized selector", and `objc_msgSendSuper` / direct-IMP workarounds also fail. Use only `-hitTest:` (one arg) with `[super hitTest:point]`. See [SDL / WKWebView hitTest routing](incidents/2026-04-06-sdl-webview-hittest-mouse-routing.md).

- **Use `NSEvent` local monitors to stamp per-gesture state before `hitTest:` runs.** Monitors fire inside `[NSApp sendEvent:]` before window routing; setting a flag in the monitor and reading it in `hitTest:` is the reliable pattern for gesture-aware hit testing. See [SDL / WKWebView hitTest routing](incidents/2026-04-06-sdl-webview-hittest-mouse-routing.md).
