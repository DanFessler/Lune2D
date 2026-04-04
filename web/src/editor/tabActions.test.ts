import { describe, expect, it, vi } from "vitest";
import { inspectorTabActions, sceneHierarchyTabActions } from "./tabActions";
import type { EngineEntity } from "../engineBridge";

describe("sceneHierarchyTabActions", () => {
  it("invokes onNewEntity when New entity is chosen", () => {
    const onNewEntity = vi.fn();
    const actions = sceneHierarchyTabActions({ onNewEntity });
    const item = actions[0]?.items[0];
    expect(item && "label" in item).toBe(true);
    expect(item && "label" in item ? item.label : "").toBe("New entity");
    if (item && "onClick" in item) item.onClick();
    expect(onNewEntity).toHaveBeenCalledTimes(1);
  });
});

describe("inspectorTabActions", () => {
  it("is undefined when no entity is selected", () => {
    expect(
      inspectorTabActions({
        entity: null,
        behaviorNames: ["Foo"],
        onNewScript: vi.fn(),
      }),
    ).toBeUndefined();
  });

  it("includes New script and Add Behavior when an entity is selected", () => {
    const entity: EngineEntity = {
      id: "7",
      name: "Ship",
      active: true,
      drawOrder: 0,
      updateOrder: 0,
      components: [],
    };
    const actions = inspectorTabActions({
      entity,
      behaviorNames: ["Bar"],
      onNewScript: vi.fn(),
    });
    expect(actions).toBeDefined();
    const top = actions![0].items;
    expect(top[0] && "label" in top[0] ? top[0].label : "").toBe("New script…");
    expect(top[1] && "label" in top[1] ? top[1].label : "").toBe("Add Behavior");
  });
});
