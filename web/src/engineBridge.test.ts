import { describe, expect, it } from "vitest";
import {
  applyOptimisticHierarchyDrop,
  buildEntityHierarchy,
  compareEntitySiblingOrder,
  getEntityTransform,
  hierarchyLayoutSignature,
  engineEntityListsSnapshotEqual,
  normalizeEngineEntityPayload,
  type BehaviorComponent,
  type EngineEntity,
} from "./engineBridge";

function ent(
  id: string,
  opts: Partial<
    Pick<EngineEntity, "parentId" | "drawOrder" | "updateOrder" | "name">
  > & { name?: string } = {},
): EngineEntity {
  return {
    id,
    name: opts.name ?? `E${id}`,
    active: true,
    drawOrder: opts.drawOrder ?? 0,
    updateOrder: opts.updateOrder ?? 0,
    parentId: opts.parentId,
    components: [
      {
        type: "Behavior",
        name: "Transform",
        isNative: true,
        propertyValues: { x: 0, y: 0, angle: 0, vx: 0, vy: 0, sx: 1, sy: 1 },
      } as BehaviorComponent,
    ],
  };
}

describe("engineEntityListsSnapshotEqual", () => {
  it("is true for identical content with distinct object references", () => {
    const a = [ent("1"), ent("2")];
    const b = [
      { ...ent("1") },
      {
        ...ent("2"),
        components: [{ ...ent("2").components[0] } as BehaviorComponent],
      },
    ];
    expect(engineEntityListsSnapshotEqual(a, b)).toBe(true);
    expect(a[0]).not.toBe(b[0]);
  });

  it("is false when a numeric field changes", () => {
    const a = [ent("1")];
    const b = [
      {
        ...ent("1"),
        components: [
          {
            type: "Behavior" as const,
            name: "Transform",
            isNative: true,
            propertyValues: {
              x: 1,
              y: 0,
              angle: 0,
              vx: 0,
              vy: 0,
              sx: 1,
              sy: 1,
            },
          },
        ],
      },
    ];
    expect(engineEntityListsSnapshotEqual(a, b)).toBe(false);
  });
});

describe("compareEntitySiblingOrder", () => {
  it("orders by drawOrder then numeric id", () => {
    const a = ent("2", { drawOrder: 0 });
    const b = ent("10", { drawOrder: 0 });
    expect(compareEntitySiblingOrder(a, b)).toBeLessThan(0);
    expect(
      compareEntitySiblingOrder(ent("1", { drawOrder: 1 }), a),
    ).toBeGreaterThan(0);
  });
});

describe("hierarchyLayoutSignature", () => {
  it("is stable for same layout regardless of array order", () => {
    const a: EngineEntity[] = [
      ent("10", { drawOrder: 0 }),
      ent("1", { drawOrder: 0, parentId: "10" }),
    ];
    const b: EngineEntity[] = [
      ent("1", { drawOrder: 0, parentId: "10" }),
      ent("10", { drawOrder: 0 }),
    ];
    expect(hierarchyLayoutSignature(a)).toBe(hierarchyLayoutSignature(b));
  });
});

describe("applyOptimisticHierarchyDrop", () => {
  it("reparents and reindexes for center drop", () => {
    const flat: EngineEntity[] = [
      ent("1", { drawOrder: 0 }),
      ent("2", { drawOrder: 0 }),
    ];
    const next = applyOptimisticHierarchyDrop(flat, "2", "1", undefined)!;
    const byId = Object.fromEntries(next.map((e) => [e.id, e]));
    expect(byId["2"].parentId).toBe("1");
    expect(byId["2"].drawOrder).toBe(0);
  });
});

describe("buildEntityHierarchy", () => {
  it("sorts siblings by drawOrder (snapshot is id-sorted from native)", () => {
    const flat: EngineEntity[] = [
      ent("1", { drawOrder: 0, parentId: "10" }),
      ent("2", { drawOrder: 2, parentId: "10" }),
      ent("3", { drawOrder: 1, parentId: "10" }),
      ent("10", { drawOrder: 0 }),
    ];
    const roots = buildEntityHierarchy(flat);
    expect(roots.map((r) => r.id)).toEqual(["10"]);
    expect(roots[0].children.map((c) => c.id)).toEqual(["1", "3", "2"]);
  });
});

describe("unified behavior model (Phase 1 TDD)", () => {
  it("normalizes Behavior-type components from native snapshot", () => {
    const raw = [
      {
        id: "1",
        name: "Ship",
        active: true,
        drawOrder: 0,
        updateOrder: 0,
        components: [
          {
            type: "Behavior",
            name: "Transform",
            isNative: true,
            propertyValues: { x: 10, y: 20, angle: 0, vx: 0, vy: 0, sx: 1, sy: 1 },
          },
          {
            type: "Behavior",
            name: "Ship",
            isNative: false,
            propertyValues: { speed: 5 },
            propertySchema: [{ name: "speed", type: "number" }],
          },
        ],
      },
    ];
    const entities = normalizeEngineEntityPayload(raw);
    expect(entities).toHaveLength(1);
    const e = entities[0];
    expect(e.components).toHaveLength(2);

    const t = e.components[0] as BehaviorComponent;
    expect(t.type).toBe("Behavior");
    expect(t.name).toBe("Transform");
    expect(t.isNative).toBe(true);

    const s = e.components[1] as BehaviorComponent;
    expect(s.type).toBe("Behavior");
    expect(s.name).toBe("Ship");
    expect(s.isNative).toBe(false);
  });

  it("getEntityTransform works with Behavior-type Transform", () => {
    const e = ent("1");
    const t = getEntityTransform(e);
    expect(t).not.toBeNull();
    expect(t!.x).toBe(0);
  });

  it("entity can have no Transform behavior", () => {
    const e: EngineEntity = {
      id: "1",
      name: "Folder",
      active: true,
      drawOrder: 0,
      updateOrder: 0,
      components: [],
    };
    expect(getEntityTransform(e)).toBeNull();
  });
});
