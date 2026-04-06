# Incident: SDL ↔ WKWebView hitTest mouse-routing breaks web gestures and crashes

**Date:** 2026-04-06  
**Severity / impact:** user-visible — editor crash on hover, broken divider/tab drags, stuck Monaco scroll, unresponsive game gizmos  
**Status:** resolved  

## Summary

The WKWebView overlay and SDL compete for mouse events via AppKit's `hitTest:` mechanism. The original passthrough (`return nil` for points inside the game viewport) broke web gestures that crossed the game hole (dock divider drags, tab DnD highlights, Monaco wheel scroll). Four successive fix attempts each broke something else because they relied on `-hitTest:withEvent:`, a **non-public** `NSView` method that WKWebView does not implement — causing crashes and silent misrouting. The working fix uses only the one-argument `-hitTest:` with an `NSEvent` local monitor to track drag origin.

## Timeline

1. **Reported:** Three mouse-input bugs — Monaco scroll stuck, dockable divider drag dies when crossing game rect, tab drop highlight missing over game tab bar.
2. **First fix:** Added drag-origin enum + `hitTest:withEvent:` override → **crash** (`unrecognized selector`) because WKWebView doesn't respond to `hitTest:withEvent:`.
3. **Second fix:** Removed `hitTest:` forwarding, kept `hitTest:withEvent:` with `objc_msgSendSuper` to `[NSView class]` → **crash** (wrong super_class in `objc_msgSendSuper`, undefined behavior on arm64).
4. **Third fix:** Called `NSView`'s IMP via `class_getInstanceMethod` + `method_getImplementation` → **no crash but web layer unresponsive** (NSView's implementation doesn't walk WKWebView's internal subview tree correctly).
5. **Fourth fix:** Added `hitTest:` back as forwarder to `hitTest:withEvent:nil` → **crash again** on cursor updates (AppKit's `_routeCursorUpdateEvent` calls `hitTest:` → our forwarding → `hitTest:withEvent:` → super dispatch → unrecognized selector).
6. **Final fix:** Removed all `hitTest:withEvent:` usage. One-arg `hitTest:` + `NSEvent` local monitor for mouseDown origin. Both web and game interaction work; no crashes.

## What happened

`Lune2DWebView` (a `WKWebView` subclass) is overlaid on SDL's content view. The game viewport is a transparent "hole" — `hitTest:` returns `nil` for points inside it so AppKit routes events to SDL underneath.

When a web-originated gesture (divider resize, tab drag, scroll) moved the pointer into the game hole, `hitTest:` returned `nil`, routing subsequent events to SDL. The web layer lost the gesture mid-flight: dividers froze, Monaco stopped scrolling, drop highlights vanished.

## Why it was hard to solve

- **`hitTest:withEvent:` does not exist on WKWebView.** It's not documented as a public `NSView` API. The original code had both overrides and the `withEvent:` version was effectively dead code (never called in practice). The compiler emitted `-Wmay-not-respond` warnings but no error.
- **Four different failure modes from four super-dispatch strategies.** Each approach compiled but crashed or silently broke at runtime: `[super hitTest:withEvent:]` (unrecognized selector), `objc_msgSendSuper` with wrong class (UB / crash), direct IMP call (misses WKWebView's subview tree), forwarding from `hitTest:` (recursive crash through `_routeCursorUpdateEvent`).
- **The "fix one, break another" cycle** obscured the real constraint: the entire `hitTest:withEvent:` codepath must be avoided. Each attempt added complexity around the wrong method instead of stepping back.
- **Testing is manual.** No automated test exercises AppKit hit-testing with a live WKWebView + SDL overlay; each iteration required a build + interactive test cycle.

## Root cause

Two independent issues:

1. **No gesture tracking in `hitTest:`.** The original `hitTest:` used only the current cursor position. During any web gesture that crossed the game rect, events switched from WKWebView to SDL, breaking the gesture.

2. **`hitTest:withEvent:` is not part of the WKWebView/NSView public contract.** Every fix that overrode or called `super` on this method crashed or misrouted because WKWebView doesn't implement it, and calling NSView's internal implementation directly doesn't traverse WKWebView's subview hierarchy.

## Resolution

**`src/webview_host_mac.mm`:**

- **Removed** all `hitTest:withEvent:` overrides, `objc_msgSendSuper` hacks, and IMP-lookup helpers.
- **`hitTest:` (one-arg)** — the only override. Three paths:
  - `pressedMouseButtons == 0` → clear `s_web_drag_active`; standard passthrough (nil for game hole, super otherwise).
  - `s_web_drag_active == YES` → `[super hitTest:point]` unconditionally (web keeps receiving events over the game hole).
  - Default → standard passthrough.
- **`NSEvent` local monitor** for `NSEventMaskLeftMouseDown | RightMouseDown | OtherMouseDown`. Fires inside `[NSApp sendEvent:]` *before* the window routes the event, so `s_web_drag_active` is set by the time `hitTest:` runs. If the mouseDown is NOT in the game passthrough rect, sets `s_web_drag_active = YES`.
- Flag is cleared on all-buttons-released (in `hitTest:`), overlay hide (toggle_visibility), and shutdown.

**`src/webview_viewport.cpp` / `src/main.cpp`:**

- Added `clamp_to_game_viewport` parameter to `webview_window_mouse_to_luau` so editor drags that briefly leave the inset still update Luau mouse coordinates (clamped to viewport edge).

## Mitigations for the future

- [ ] Add a comment on `Lune2DWebView` that `hitTest:withEvent:` must never be overridden (it's not a public NSView API and WKWebView doesn't implement it).
- [ ] Consider a CI smoke test that launches lune2d with `--capture-after-frames` and a synthetic mouse-hover event to detect startup crashes.
- [ ] If further hit-test edge cases appear, investigate an `NSTrackingArea`-based approach instead of hitTest overrides.

## Agent / contributor documentation

- **Never override `-hitTest:withEvent:` on a WKWebView subclass.** It is not a public `NSView` method. WKWebView does not implement it. `[super hitTest:point withEvent:event]` will crash with "unrecognized selector". All `objc_msgSendSuper` and direct-IMP workarounds also fail (wrong subview traversal or UB). Use only `-hitTest:` (one argument).
- **Use `NSEvent` local monitors to stamp state before hitTest.** Local monitors fire inside `[NSApp sendEvent:]` before the event reaches the window and triggers `hitTest:`. This is the reliable way to set per-gesture routing flags.
- **`[NSEvent pressedMouseButtons]` is live physical state**, safe to query in `hitTest:` for clearing stale flags, but unreliable for detecting the *start* of a press (the button may already be down by the time `hitTest:` runs, or the event sequence may not match expectations).
