import { useCallback, useEffect, useMemo, useState, type ReactNode } from "react";
import { flushSync } from "react-dom";
import { FaSearch } from "react-icons/fa";
import type { EngineEntity, HierarchyEntity } from "../../engineBridge";
import {
  applyOptimisticHierarchyDrop,
  buildEntityHierarchy,
  compareEntitySiblingOrder,
  hierarchyLayoutSignature,
} from "../../engineBridge";
import { engine } from "../../engineProxy";
import HierarchyList from "../components/HierarchyList";
import styles from "./Hierarchy.module.css";
import {
  closestCenter,
  DndContext,
  DragOverlay,
  PointerSensor,
  useSensor,
  useSensors,
  type DragEndEvent,
} from "@dnd-kit/core";

export type SceneHierarchyProps = {
  entities: EngineEntity[];
  selectedId: string | null;
  onSelect: (id: string) => void;
};

function findHierarchyNode(
  nodes: HierarchyEntity[],
  id: string,
): HierarchyEntity | null {
  for (const n of nodes) {
    if (n.id === id) return n;
    const found = findHierarchyNode(n.children, id);
    if (found) return found;
  }
  return null;
}

/** Destroys children before parent so we do not leave dangling `parentId` references. */
function collectPostOrderEntityIds(node: HierarchyEntity): string[] {
  const out: string[] = [];
  for (const c of node.children) out.push(...collectPostOrderEntityIds(c));
  out.push(node.id);
  return out;
}

function filterHierarchy(
  nodes: HierarchyEntity[],
  query: string,
): HierarchyEntity[] {
  const q = query.trim().toLowerCase();
  if (!q) return nodes;

  const filt = (n: HierarchyEntity): HierarchyEntity | null => {
    const selfMatch = n.name.toLowerCase().includes(q);
    const filteredChildren = n.children
      .map(filt)
      .filter((c): c is HierarchyEntity => c != null);
    if (selfMatch) {
      return { ...n, children: n.children };
    }
    if (filteredChildren.length > 0) {
      return { ...n, children: filteredChildren };
    }
    return null;
  };

  return nodes.map(filt).filter((n): n is HierarchyEntity => n != null);
}

export default function SceneHierarchy({
  entities,
  selectedId,
  onSelect,
}: SceneHierarchyProps) {
  const [search, setSearch] = useState("");
  const [activeChildren, setActiveChildren] = useState<ReactNode | null>(null);
  const [optimisticEntities, setOptimisticEntities] = useState<
    EngineEntity[] | null
  >(null);

  const modelEntities = optimisticEntities ?? entities;

  useEffect(() => {
    if (!optimisticEntities) return;
    if (hierarchyLayoutSignature(entities) === hierarchyLayoutSignature(optimisticEntities)) {
      setOptimisticEntities(null);
    }
  }, [entities, optimisticEntities]);

  const tree = useMemo(
    () => buildEntityHierarchy(modelEntities),
    [modelEntities],
  );

  const filteredTree = useMemo(
    () => filterHierarchy(tree, search),
    [tree, search],
  );

  const sensors = useSensors(
    useSensor(PointerSensor, {
      activationConstraint: { distance: 10 },
    }),
  );

  const deleteEntitySubtree = useCallback(
    async (rootId: string) => {
      const node = findHierarchyNode(tree, rootId);
      if (!node) return;
      for (const id of collectPostOrderEntityIds(node)) {
        try {
          await engine.runtime.destroy(Number(id));
        } catch (e) {
          console.error(e);
        }
      }
    },
    [tree],
  );

  function handleDragEnd(event: DragEndEvent) {
    const active = event.active.data?.current;
    const current = event.over?.data.current;
    if (!current || !active) return;

    const { objectId: activeId } = active;
    const { objectId: targetId, side } = current;

    if (!activeId || !targetId || activeId === targetId) return;

    const base = optimisticEntities ?? entities;
    const optimistic = applyOptimisticHierarchyDrop(
      base,
      activeId,
      targetId,
      side === "top" || side === "bottom" ? side : undefined,
    );
    if (optimistic) {
      flushSync(() => {
        setOptimisticEntities(optimistic);
      });
    }

    const activeEntity = base.find((e) => e.id === activeId);
    const targetEntity = base.find((e) => e.id === targetId);
    if (!activeEntity || !targetEntity) return;

    if (side === "top" || side === "bottom") {
      const targetParentId = targetEntity.parentId
        ? Number(targetEntity.parentId)
        : 0;
      if (targetParentId > 0) {
        engine.runtime.setParent(Number(activeId), targetParentId);
      } else {
        engine.runtime.removeParent(Number(activeId));
      }
      const siblings = base
        .filter(
          (e) =>
            (e.parentId ?? null) === (targetEntity.parentId ?? null) &&
            e.id !== activeId,
        )
        .sort(compareEntitySiblingOrder);
      const targetIdx = siblings.findIndex((e) => e.id === targetId);
      const insertIdx = side === "top" ? targetIdx : targetIdx + 1;
      siblings.splice(insertIdx, 0, activeEntity);
      siblings.forEach((e, i) => {
        engine.runtime.setDrawOrder(Number(e.id), i);
        engine.runtime.setUpdateOrder(Number(e.id), i);
      });
    } else {
      const parentKey = String(targetId);
      engine.runtime.setParent(Number(activeId), Number(targetId));
      const existingUnderTarget = base
        .filter((e) => (e.parentId ?? null) === parentKey && e.id !== activeId)
        .sort(compareEntitySiblingOrder);
      const underTarget = [...existingUnderTarget, activeEntity];
      underTarget.forEach((e, i) => {
        engine.runtime.setDrawOrder(Number(e.id), i);
        engine.runtime.setUpdateOrder(Number(e.id), i);
      });
    }
  }

  return (
    <DndContext
      sensors={sensors}
      collisionDetection={closestCenter}
      onDragStart={({ active }) => {
        setActiveChildren(active.data?.current?.children);
      }}
      onDragEnd={handleDragEnd}
      onDragCancel={() => setActiveChildren(null)}
    >
      <div className={styles.container}>
        <div className={styles.header}>
          <div className={styles.searchContainer}>
            <FaSearch className={styles.searchIcon} />
            <input
              type="text"
              placeholder="Search…"
              className={styles.searchInput}
              value={search}
              onChange={(e) => setSearch(e.target.value)}
              aria-label="Search hierarchy"
            />
          </div>
        </div>
        <div className={styles.content}>
          <HierarchyList
            entities={filteredTree}
            selectedId={selectedId}
            onSelect={onSelect}
            onDeleteEntity={deleteEntitySubtree}
            canDrop={true}
          />
        </div>
      </div>
      <DragOverlay>
        {activeChildren}
      </DragOverlay>
    </DndContext>
  );
}
