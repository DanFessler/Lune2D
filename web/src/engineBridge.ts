export type TransformComponent = {
  type: "Transform";
  x: number;
  y: number;
  angle: number;
  vx: number;
  vy: number;
};

/** Behavior property field metadata (from native schema). */
export type BehaviorPropertyField = {
  name: string;
  type: string;
  default?: unknown;
  min?: number;
  max?: number;
  /** When true (from prop.number/integer with slider + min/max), inspector shows a range control. */
  slider?: boolean;
  enumOptions?: string[];
};

export type ScriptComponent = {
  type: "Script";
  behavior: string;
  /** Serialized per-instance overrides only (may be empty). */
  properties?: Record<string, unknown>;
  /** Merged values (defaults + overrides) for editor display. */
  propertyValues?: Record<string, unknown>;
  propertySchema?: BehaviorPropertyField[];
};

export type EngineComponent = TransformComponent | ScriptComponent;

export type EngineEntity = {
  id: string;
  name: string;
  active: boolean;
  drawOrder: number;
  updateOrder: number;
  /** When set, builds a nested hierarchy in the scene tree (optional; native may omit). */
  parentId?: string | null;
  components: EngineComponent[];
};

/** Entity node for hierarchy UI (children populated when parentId links exist). */
export type HierarchyEntity = EngineEntity & {
  children: HierarchyEntity[];
};

/** Legacy flat row from older native snapshots (optional compat). */
type LegacyFlatEntity = {
  id: string;
  name: string;
  x: number;
  y: number;
  angle: number;
  vx: number;
  vy: number;
};

function isTransformComponent(c: EngineComponent): c is TransformComponent {
  return c.type === "Transform";
}

export function getEntityTransform(e: EngineEntity): TransformComponent | null {
  const t = e.components.find(isTransformComponent);
  return t ?? null;
}

function normalizeEntity(row: unknown): EngineEntity | null {
  if (!row || typeof row !== "object") return null;
  const r = row as Record<string, unknown>;
  const id = r.id != null ? String(r.id) : "";
  if (!id) return null;

  if (Array.isArray(r.components)) {
    const parentRaw = r.parentId;
    return {
      id,
      name: typeof r.name === "string" ? r.name : "Entity",
      active: r.active !== false,
      drawOrder: typeof r.drawOrder === "number" ? r.drawOrder : 0,
      updateOrder: typeof r.updateOrder === "number" ? r.updateOrder : 0,
      parentId:
        parentRaw === null || parentRaw === undefined
          ? undefined
          : String(parentRaw),
      components: r.components as EngineComponent[],
    };
  }

  const legacy = r as unknown as LegacyFlatEntity;
  return {
    id,
    name: typeof legacy.name === "string" ? legacy.name : "Entity",
    active: true,
    drawOrder: 0,
    updateOrder: 0,
    parentId: undefined,
    components: [
      {
        type: "Transform",
        x: Number(legacy.x) || 0,
        y: Number(legacy.y) || 0,
        angle: Number(legacy.angle) || 0,
        vx: Number(legacy.vx) || 0,
        vy: Number(legacy.vy) || 0,
      },
    ],
  };
}

export function normalizeEngineEntityPayload(raw: unknown): EngineEntity[] {
  if (!Array.isArray(raw)) return [];
  const out: EngineEntity[] = [];
  for (const row of raw) {
    const e = normalizeEntity(row);
    if (e) out.push(e);
  }
  return out;
}

/** Match native `forEachEntitySortedByDrawOrder`: drawOrder asc, then id asc. */
export function compareEntitySiblingOrder(
  a: EngineEntity,
  b: EngineEntity,
): number {
  if (a.drawOrder !== b.drawOrder) return a.drawOrder - b.drawOrder;
  return Number(a.id) - Number(b.id);
}

/** Stable layout fingerprint for optimistic–native reconciliation. */
export function hierarchyLayoutSignature(entities: EngineEntity[]): string {
  return [...entities]
    .sort((a, b) => Number(a.id) - Number(b.id))
    .map(
      (e) => `${e.id}\t${e.parentId ?? ""}\t${e.drawOrder}\t${e.updateOrder}`,
    )
    .join("\n");
}

/**
 * Local mirror of hierarchy drag logic so the UI can update before the next native snapshot.
 * Keeps @dnd-kit drop animation aligned with ctx-game (draggable node is already at the new row).
 */
export function applyOptimisticHierarchyDrop(
  entities: EngineEntity[],
  activeId: string,
  targetId: string,
  side: "top" | "bottom" | undefined,
): EngineEntity[] | null {
  const activeEntity = entities.find((e) => e.id === activeId);
  const targetEntity = entities.find((e) => e.id === targetId);
  if (!activeEntity || !targetEntity) return null;

  const next: EngineEntity[] = entities.map((e) => ({ ...e }));

  const patch = (id: string, fn: (e: EngineEntity) => void) => {
    const row = next.find((x) => x.id === id);
    if (row) fn(row);
  };

  if (side === "top" || side === "bottom") {
    const parentRaw = targetEntity.parentId;
    const parentStr =
      parentRaw === undefined || parentRaw === null ? null : String(parentRaw);
    patch(activeId, (e) => {
      e.parentId = parentStr === null ? undefined : parentStr;
    });
    const activeRow = next.find((e) => e.id === activeId)!;
    const siblings = next
      .filter(
        (e) =>
          (e.parentId ?? null) === (targetEntity.parentId ?? null) &&
          e.id !== activeId,
      )
      .sort(compareEntitySiblingOrder);
    const targetIdx = siblings.findIndex((e) => e.id === targetId);
    const insertIdx = side === "top" ? targetIdx : targetIdx + 1;
    const ordered = [
      ...siblings.slice(0, insertIdx),
      activeRow,
      ...siblings.slice(insertIdx),
    ];
    ordered.forEach((e, i) => {
      patch(e.id, (row) => {
        row.drawOrder = i;
        row.updateOrder = i;
      });
    });
  } else {
    patch(activeId, (e) => {
      e.parentId = targetId;
    });
    const activeRow = next.find((e) => e.id === activeId)!;
    const others = next
      .filter((e) => (e.parentId ?? null) === targetId && e.id !== activeId)
      .sort(compareEntitySiblingOrder);
    const ordered = [...others, activeRow];
    ordered.forEach((e, i) => {
      patch(e.id, (row) => {
        row.drawOrder = i;
        row.updateOrder = i;
      });
    });
  }

  return next;
}

/** True if `childId` appears on the parentId chain walking up from `parentId` (cycle guard). */
function childIsReachableAscendingFromParent(
  childId: string,
  parentId: string,
  byId: Map<string, EngineEntity>,
): boolean {
  let cur: string | undefined = parentId;
  const seen = new Set<string>();
  while (cur) {
    if (cur === childId) return true;
    if (seen.has(cur)) break;
    seen.add(cur);
    const ent = byId.get(cur);
    const next = ent?.parentId;
    cur = next && next !== cur ? String(next) : undefined;
  }
  return false;
}

/** Build a forest from optional parentId links; orphans, missing parents, and cycles break to roots. */
export function buildEntityHierarchy(
  entities: EngineEntity[],
): HierarchyEntity[] {
  const byId = new Map(entities.map((e) => [e.id, e]));
  const nodes = new Map<string, HierarchyEntity>();
  for (const e of entities) {
    nodes.set(e.id, { ...e, children: [] });
  }
  const roots: HierarchyEntity[] = [];
  for (const e of entities) {
    const node = nodes.get(e.id);
    if (!node) continue;
    const p = e.parentId ? String(e.parentId) : "";
    const parentOk =
      !!p &&
      p !== e.id &&
      nodes.has(p) &&
      !childIsReachableAscendingFromParent(e.id, p, byId);
    if (parentOk) {
      nodes.get(p)!.children.push(node);
    } else {
      roots.push(node);
    }
  }

  function sortChildren(nodes: HierarchyEntity[]) {
    nodes.sort(compareEntitySiblingOrder);
    for (const n of nodes) {
      if (n.children.length > 0) sortChildren(n.children);
    }
  }
  sortChildren(roots);

  return roots;
}

declare global {
  interface Window {
    __engineOnEntities?: (payload: EngineEntity[]) => void;
    /** Native viewport pick: sync hierarchy/inspector selection. */
    __engineSelectEntity?: (id: number | string | null) => void;
  }
}

/** Installs `window.__engineOnEntities` and returns an unsubscribe. */
export function installEngineEntityBridge(
  onEntities: (list: EngineEntity[]) => void,
): () => void {
  const handler = (payload: unknown) => {
    onEntities(normalizeEngineEntityPayload(payload));
  };
  window.__engineOnEntities = handler as (p: EngineEntity[]) => void;
  return () => {
    if (window.__engineOnEntities === handler) {
      delete window.__engineOnEntities;
    }
  };
}
