# Plan: Native behavior descriptor registry

**Status:** proposed
**Triggered by:** Native behavior parity follow-up

## Goal

Replace the remaining `Transform`-specific native behavior property branches with a small descriptor/registry layer so future native behaviors can participate in the shared schema/editor/property pipeline without adding more `if (slot.name == "...")` checks.

## Non-goals

- Moving native behavior state out of C++ storage.
- Making native behaviors run through Luau behavior dispatch.
- Rewriting world-matrix, picking, or hierarchy math away from native `Transform`.

## Proposed shape

- Define a native behavior descriptor per behavior name with:
  - schema registration
  - property snapshot serialization
  - property mutation/apply
  - optional save/load helpers if needed later
- Keep `BehaviorSlot` native payloads native; the descriptor is just the bridge between native storage and the shared behavior-property interface.

## Plan (ordered steps)

1. Introduce a native descriptor lookup API keyed by behavior name.
2. Move `Transform` schema/property getter/property setter into that descriptor.
3. Update `eng_native_attachable_behavior_names()` to derive from the descriptor registry instead of a separate string array.
4. Route editor snapshot, property mutation, and scene load/save through the descriptor API.
5. Add one focused test with a fake second native behavior shape or a descriptor-level unit test to prevent regressions back to name-based branching.

## Risks and regression checks

- **Risk:** Duplicating behavior name lists between descriptor registration and attachable behavior menus.
- **Risk:** Making the descriptor too abstract before a second native behavior exists.
- **Check:** Add another native behavior should require registering one descriptor, not touching multiple switch sites.

## Rollout

Single refactor pass is fine; keep `Transform` behavior identical externally while moving its bridging logic behind the registry.
