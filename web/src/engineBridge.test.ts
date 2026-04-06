import { describe, expect, it } from "vitest";
import {
  applyOptimisticHierarchyDrop,
  buildEntityHierarchy,
  compareEntitySiblingOrder,
  hierarchyLayoutSignature,
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
    components: [{ type: "Transform", x: 0, y: 0, angle: 0, vx: 0, vy: 0 }],
  };
}

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
