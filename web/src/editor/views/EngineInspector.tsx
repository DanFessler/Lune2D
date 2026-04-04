import { useEffect, useMemo, useState, type ReactNode } from "react";
import type { EngineEntity, TransformComponent } from "../../engineBridge";
import { getEntityTransform } from "../../engineBridge";
import { engine } from "../../engineProxy";
import { PiBoundingBoxFill } from "react-icons/pi";
import { FaCode, FaTimes } from "react-icons/fa";
import {
  DndContext,
  closestCorners,
  PointerSensor,
  useSensor,
  useSensors,
  DragOverlay,
  type DragEndEvent,
} from "@dnd-kit/core";
import {
  SortableContext,
  verticalListSortingStrategy,
} from "@dnd-kit/sortable";
import { restrictToVerticalAxis } from "@dnd-kit/modifiers";
import SortableItem from "../components/SortableItem";
import styles from "./Inspector.module.css";

function useComponentCollapseMap(keysSig: string) {
  const [collapseMap, setCollapseMap] = useState<Record<string, boolean>>({});

  useEffect(() => {
    const keys = keysSig ? keysSig.split("\0") : [];
    setCollapseMap((prev) => {
      const next = { ...prev };
      for (const k of keys) {
        if (k && !(k in next)) next[k] = false;
      }
      for (const k of Object.keys(next)) {
        if (!keys.includes(k)) delete next[k];
      }
      return next;
    });
  }, [keysSig]);

  function handleFold(key: string, isCollapsed: boolean) {
    setCollapseMap((m) => ({ ...m, [key]: isCollapsed }));
  }

  return { collapseMap, handleFold };
}

function CollapsibleBlock({
  name,
  icon,
  canDisable,
  active,
  isCollapsed,
  onFold,
  onToggleActive,
  onRemove,
  children,
}: {
  name: string;
  icon: ReactNode;
  canDisable: boolean;
  active: boolean;
  isCollapsed: boolean;
  onFold: (collapsed: boolean) => void;
  onToggleActive?: () => void;
  onRemove?: () => void;
  children: ReactNode;
}) {
  return (
    <div
      className={styles.behaviorContainer}
      style={{ boxShadow: "0 -1px 0 rgba(0, 0, 0, 0.1)" }}
    >
      <div
        className={styles.behaviorHeader}
        onClick={() => onFold(!isCollapsed)}
      >
        <input
          type="checkbox"
          checked={active}
          disabled={!canDisable}
          aria-label={`${name} active`}
          onChange={() => onToggleActive?.()}
          onClick={(e) => e.stopPropagation()}
        />
        {icon}
        <div>{name}</div>
        <div className={styles.spacer} />
        {onRemove ? (
          <button
            className={styles.removeBtn}
            aria-label={`Remove ${name}`}
            onClick={(e) => { e.stopPropagation(); onRemove(); }}
          >
            <FaTimes />
          </button>
        ) : null}
        <div className={styles.arrowContainer}>{isCollapsed ? "▶" : "▼"}</div>
      </div>
      {!isCollapsed ? <div className={styles.behaviorContent}>{children}</div> : null}
    </div>
  );
}

function EditableVec2({
  label,
  x,
  y,
  onCommitX,
  onCommitY,
}: {
  label: string;
  x: number;
  y: number;
  onCommitX: (v: number) => void;
  onCommitY: (v: number) => void;
}) {
  const [lx, setLx] = useState(String(x));
  const [ly, setLy] = useState(String(y));
  useEffect(() => setLx(String(x)), [x]);
  useEffect(() => setLy(String(y)), [y]);

  const commitX = () => {
    const v = parseFloat(lx);
    if (Number.isFinite(v) && v !== x) onCommitX(v);
    else setLx(String(x));
  };
  const commitY = () => {
    const v = parseFloat(ly);
    if (Number.isFinite(v) && v !== y) onCommitY(v);
    else setLy(String(y));
  };

  return (
    <>
      <span className={styles.fieldLabel}>{label}</span>
      <input
        className={styles.fieldInput}
        type="number"
        value={lx}
        onChange={(e) => setLx(e.target.value)}
        onBlur={commitX}
        onKeyDown={(e) => e.key === "Enter" && commitX()}
        aria-label={`${label} X`}
      />
      <input
        className={styles.fieldInput}
        type="number"
        value={ly}
        onChange={(e) => setLy(e.target.value)}
        onBlur={commitY}
        onKeyDown={(e) => e.key === "Enter" && commitY()}
        aria-label={`${label} Y`}
      />
    </>
  );
}

function EditableFloat({
  label,
  value,
  onCommit,
}: {
  label: string;
  value: number;
  onCommit: (v: number) => void;
}) {
  const [local, setLocal] = useState(String(value));
  useEffect(() => setLocal(String(value)), [value]);

  const commit = () => {
    const v = parseFloat(local);
    if (Number.isFinite(v) && v !== value) onCommit(v);
    else setLocal(String(value));
  };

  return (
    <>
      <span className={styles.fieldLabel}>{label}</span>
      <input
        className={`${styles.fieldInput} ${styles.fieldSpan2}`}
        type="number"
        value={local}
        onChange={(e) => setLocal(e.target.value)}
        onBlur={commit}
        onKeyDown={(e) => e.key === "Enter" && commit()}
      />
    </>
  );
}

function TransformFields({ t, entityId }: { t: TransformComponent; entityId: number }) {
  const set = (field: string) => (v: number) =>
    engine.runtime.setTransform(entityId, field, v);
  return (
    <>
      <EditableVec2 label="Position" x={t.x} y={t.y} onCommitX={set("x")} onCommitY={set("y")} />
      <EditableFloat label="Angle" value={t.angle} onCommit={set("angle")} />
      <EditableVec2 label="Velocity" x={t.vx} y={t.vy} onCommitX={set("vx")} onCommitY={set("vy")} />
    </>
  );
}

function ScriptList({ entity, collapseMap, onFold }: {
  entity: EngineEntity;
  collapseMap: Record<string, boolean>;
  onFold: (key: string, collapsed: boolean) => void;
}) {
  const entityId = Number(entity.id);
  const [activeChildren, setActiveChildren] = useState<ReactNode | null>(null);

  const scriptComponents = entity.components
    .map((c, i) => ({ comp: c, idx: i }))
    .filter((x) => x.comp.type === "Script");

  const sensors = useSensors(
    useSensor(PointerSensor, { activationConstraint: { distance: 10 } }),
  );

  const scriptIds = scriptComponents.map((_, i) => `script-${i}`);

  function handleDragEnd(event: DragEndEvent) {
    const { active, over } = event;
    if (!over) return;
    if (active.id !== over.id) {
      const fromIdx = scriptIds.indexOf(String(active.id));
      const toIdx = scriptIds.indexOf(String(over.id));
      if (fromIdx >= 0 && toIdx >= 0) {
        engine.runtime.reorderScript(entityId, fromIdx, toIdx);
      }
    }
    setActiveChildren(null);
  }

  if (scriptComponents.length === 0) return null;

  return (
    <DndContext
      sensors={sensors}
      collisionDetection={closestCorners}
      onDragStart={({ active }) => {
        setActiveChildren(active.data?.current?.children);
      }}
      onDragEnd={handleDragEnd}
      onDragCancel={() => setActiveChildren(null)}
      modifiers={[restrictToVerticalAxis]}
    >
      <SortableContext items={scriptIds} strategy={verticalListSortingStrategy}>
        {scriptComponents.map((sc, i) => {
          if (sc.comp.type !== "Script") return null;
          const key = `script:${i}:${sc.comp.behavior}`;
          return (
            <SortableItem key={scriptIds[i]} id={scriptIds[i]} data={{ type: "behavior" }}>
              <CollapsibleBlock
                name={sc.comp.behavior}
                icon={<FaCode style={{ width: "14px", height: "14px" }} />}
                canDisable={false}
                active={true}
                isCollapsed={collapseMap[key] ?? false}
                onFold={(c) => onFold(key, c)}
                onRemove={() => engine.runtime.removeScript(entityId, i)}
              >
                <span
                  className={styles.fieldLabel}
                  style={{ gridColumn: "1 / -1" }}
                >
                  Luau behavior
                </span>
              </CollapsibleBlock>
            </SortableItem>
          );
        })}
      </SortableContext>
      <DragOverlay>
        {activeChildren ? (
          <div
            style={{
              borderRadius: 4,
              overflow: "hidden",
              boxShadow: "0 2px 10px 0 rgba(0, 0, 0, 0.25)",
            }}
          >
            {activeChildren}
          </div>
        ) : null}
      </DragOverlay>
    </DndContext>
  );
}

export default function EngineInspector({
  entity,
}: {
  entity: EngineEntity | null;
}) {
  const transform = entity ? getEntityTransform(entity) : null;
  const entityId = entity ? Number(entity.id) : 0;

  const [localName, setLocalName] = useState(entity?.name ?? "");
  useEffect(() => setLocalName(entity?.name ?? ""), [entity?.name]);

  const commitName = () => {
    if (entity && localName !== entity.name) {
      engine.runtime.setName(entityId, localName);
    }
  };

  const foldKeysSig = useMemo(() => {
    const parts: string[] = [];
    if (transform) parts.push("__transform");
    if (entity) {
      let si = 0;
      for (const c of entity.components) {
        if (c.type === "Script") {
          parts.push(`script:${si}:${c.behavior}`);
          si++;
        }
      }
    }
    return parts.join("\0");
  }, [entity, transform]);
  const { collapseMap, handleFold } = useComponentCollapseMap(foldKeysSig);

  if (!entity) {
    return (
      <div className={styles.inspector}>
        <div className={styles.header}>
          <PiBoundingBoxFill style={{ width: "16px", height: "16px" }} />
          <span style={{ fontSize: "12px" }}>Inspector</span>
        </div>
        <p className={styles.emptyHint}>
          Select an object in the hierarchy to inspect its components.
        </p>
      </div>
    );
  }

  return (
    <div className={styles.inspector}>
      <div className={styles.header}>
        <input
          type="checkbox"
          checked={entity.active}
          aria-label="Entity active"
          onChange={() => engine.runtime.setActive(entityId, !entity.active)}
          onClick={(e) => e.stopPropagation()}
        />
        <PiBoundingBoxFill style={{ width: "16px", height: "16px" }} />
        <input
          type="text"
          value={localName}
          className={styles.nameInput}
          aria-label="Entity name"
          onChange={(e) => setLocalName(e.target.value)}
          onBlur={commitName}
          onKeyDown={(e) => e.key === "Enter" && commitName()}
        />
      </div>
      <p className={styles.metaRow}>
        id {entity.id} · draw {entity.drawOrder} · update {entity.updateOrder}
      </p>
      <div
        style={{
          overflow: "auto",
          flex: 1,
          minHeight: 0,
        }}
      >
        {transform ? (
          <CollapsibleBlock
            name="Transform"
            icon={
              <PiBoundingBoxFill style={{ width: "14px", height: "14px" }} />
            }
            canDisable={false}
            active={true}
            isCollapsed={collapseMap["__transform"] ?? false}
            onFold={(c) => handleFold("__transform", c)}
          >
            <TransformFields t={transform} entityId={entityId} />
          </CollapsibleBlock>
        ) : null}
        <ScriptList entity={entity} collapseMap={collapseMap} onFold={handleFold} />
      </div>
    </div>
  );
}
