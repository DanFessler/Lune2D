import { useCallback, useMemo, useState, type ReactNode } from "react";
import { FaPlus, FaSearch } from "react-icons/fa";
import type { EngineEntity, HierarchyEntity } from "../../engineBridge";
import { buildEntityHierarchy } from "../../engineBridge";
import { engine, unwrapSceneNumber } from "../../engineProxy";
import HierarchyList from "../components/HierarchyList";
import styles from "./Hierarchy.module.css";
import {
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
  const [spawnBusy, setSpawnBusy] = useState(false);
  const [spawnError, setSpawnError] = useState<string | null>(null);

  const tree = useMemo(
    () => buildEntityHierarchy(entities),
    [entities],
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

  function handleDragEnd(event: DragEndEvent) {
    const active = event.active.data?.current;
    const current = event.over?.data.current;
    if (!current || !active) return;

    const { entityId: activeId } = active;
    const { entityId: targetId, side } = current;

    if (!activeId || !targetId || activeId === targetId) return;

    const activeEntity = entities.find((e) => e.id === activeId);
    const targetEntity = entities.find((e) => e.id === targetId);
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
      const siblings = entities
        .filter(
          (e) =>
            (e.parentId ?? null) === (targetEntity.parentId ?? null) &&
            e.id !== activeId,
        )
        .sort((a, b) => a.drawOrder - b.drawOrder);
      const targetIdx = siblings.findIndex((e) => e.id === targetId);
      const insertIdx = side === "top" ? targetIdx : targetIdx + 1;
      siblings.splice(insertIdx, 0, activeEntity);
      siblings.forEach((e, i) => {
        if (e.drawOrder !== i) {
          engine.runtime.setDrawOrder(Number(e.id), i);
        }
      });
    } else {
      engine.runtime.setParent(Number(activeId), Number(targetId));
    }

    setActiveChildren(null);
  }

  const handleNewEntity = useCallback(async () => {
    setSpawnError(null);
    setSpawnBusy(true);
    try {
      const res = await engine.runtime.spawn("Entity");
      const id = unwrapSceneNumber(res);
      onSelect(String(id));
    } catch (e) {
      setSpawnError(e instanceof Error ? e.message : String(e));
    } finally {
      setSpawnBusy(false);
    }
  }, [onSelect]);

  return (
    <DndContext
      sensors={sensors}
      onDragStart={({ active }) => {
        setActiveChildren(active.data?.current?.children);
      }}
      onDragEnd={handleDragEnd}
      onDragCancel={() => setActiveChildren(null)}
    >
      <div className={styles.container}>
        <div className={styles.header}>
          <div className={styles.toolbarRow}>
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
            <button
              type="button"
              className={styles.newEntityBtn}
              title="New entity"
              aria-label="New entity"
              disabled={spawnBusy}
              onClick={() => void handleNewEntity()}
            >
              <FaPlus aria-hidden />
            </button>
          </div>
          {spawnError ? (
            <p className={styles.spawnError} role="alert">
              {spawnError}
            </p>
          ) : null}
        </div>
        <div className={styles.content}>
          <HierarchyList
            entities={filteredTree}
            selectedId={selectedId}
            onSelect={onSelect}
            canDrop={true}
          />
        </div>
      </div>
      <DragOverlay>
        {activeChildren ? (
          <div>{activeChildren}</div>
        ) : null}
      </DragOverlay>
    </DndContext>
  );
}
