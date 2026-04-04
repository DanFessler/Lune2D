import {
  useCallback,
  useEffect,
  useLayoutEffect,
  useMemo,
  useRef,
  useState,
  type ReactNode,
} from "react";
import { createPortal } from "react-dom";
import type { EngineEntity, TransformComponent } from "../../engineBridge";
import { getEntityTransform } from "../../engineBridge";
import { engine } from "../../engineProxy";
import {
  listBehaviors,
  reloadBehaviors,
  writeLuaFile,
} from "../../luaEditorBridge";
import {
  behaviorLifecycleTemplate,
  behaviorRelativePath,
  sanitizeBehaviorBaseName,
} from "../behaviorScriptTemplate";
import { PiBoundingBoxFill } from "react-icons/pi";
import { FaCode, FaPlus, FaTimes } from "react-icons/fa";
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

type AddMenuView = "list" | "newScriptName";

function AddComponentButton({
  entityId,
  onOpenLuaFile,
}: {
  entityId: number;
  onOpenLuaFile: (path: string, content?: string) => void;
}) {
  const [isOpen, setIsOpen] = useState(false);
  const [menuView, setMenuView] = useState<AddMenuView>("list");
  const [draftName, setDraftName] = useState("NewBehavior");
  const [nameError, setNameError] = useState<string | null>(null);
  const [behaviors, setBehaviors] = useState<string[]>([]);
  const [menuPos, setMenuPos] = useState<{
    left: number;
    width: number;
    bottom: number;
  } | null>(null);
  const anchorRef = useRef<HTMLDivElement>(null);
  const menuRef = useRef<HTMLDivElement>(null);
  const nameInputRef = useRef<HTMLInputElement>(null);

  const closeMenu = useCallback(() => {
    setIsOpen(false);
    setMenuView("list");
    setNameError(null);
  }, []);

  const fetchBehaviors = useCallback(async () => {
    try {
      setBehaviors(await listBehaviors());
    } catch {
      setBehaviors([]);
    }
  }, []);

  useEffect(() => {
    if (isOpen) fetchBehaviors();
  }, [isOpen, fetchBehaviors]);

  const updateMenuPos = useCallback(() => {
    const el = anchorRef.current;
    if (!el) return;
    const r = el.getBoundingClientRect();
    setMenuPos({
      left: r.left + 8,
      width: Math.max(140, r.width - 16),
      bottom: window.innerHeight - r.top + 4,
    });
  }, []);

  useLayoutEffect(() => {
    if (!isOpen) {
      setMenuPos(null);
      return;
    }
    updateMenuPos();
    window.addEventListener("resize", updateMenuPos);
    window.addEventListener("scroll", updateMenuPos, true);
    return () => {
      window.removeEventListener("resize", updateMenuPos);
      window.removeEventListener("scroll", updateMenuPos, true);
    };
  }, [isOpen, updateMenuPos]);

  useEffect(() => {
    if (!isOpen || menuView !== "newScriptName") return;
    nameInputRef.current?.focus();
    nameInputRef.current?.select();
  }, [isOpen, menuView]);

  useEffect(() => {
    if (!isOpen) return;
    function onPointerDown(e: PointerEvent) {
      const t = e.target as Node;
      if (anchorRef.current?.contains(t)) return;
      if (menuRef.current?.contains(t)) return;
      closeMenu();
    }
    document.addEventListener("pointerdown", onPointerDown);
    return () => document.removeEventListener("pointerdown", onPointerDown);
  }, [isOpen, closeMenu]);

  useEffect(() => {
    if (!isOpen) return;
    function onKey(e: KeyboardEvent) {
      if (e.key === "Escape") closeMenu();
    }
    window.addEventListener("keydown", onKey);
    return () => window.removeEventListener("keydown", onKey);
  }, [isOpen, closeMenu]);

  const createNewScript = useCallback(async () => {
    const base = sanitizeBehaviorBaseName(draftName);
    if (!base) {
      setNameError(
        "Invalid name. Use letters, numbers, underscores; start with a letter or _.",
      );
      return;
    }
    setNameError(null);
    const relPath = behaviorRelativePath(base);
    const template = behaviorLifecycleTemplate(base);
    try {
      await writeLuaFile(relPath, template);
      await reloadBehaviors();
      await engine.runtime.addScript(entityId, base);
      onOpenLuaFile(relPath, template);
      closeMenu();
      void fetchBehaviors();
    } catch (e) {
      setNameError(e instanceof Error ? e.message : String(e));
    }
  }, [draftName, entityId, onOpenLuaFile, fetchBehaviors, closeMenu]);

  const toggleOpen = () => {
    setIsOpen((open) => {
      if (open) {
        setMenuView("list");
        setNameError(null);
        return false;
      }
      setMenuView("list");
      setNameError(null);
      return true;
    });
  };

  const menuPortal =
    isOpen &&
    menuPos &&
    createPortal(
      <div
        ref={menuRef}
        className={styles.addComponentMenuPortal}
        style={{
          left: menuPos.left,
          width: menuPos.width,
          bottom: menuPos.bottom,
        }}
        role="menu"
      >
        {menuView === "newScriptName" ? (
          <>
            <button
              type="button"
              className={styles.dropdownBackItem}
              onClick={(e) => {
                e.stopPropagation();
                setMenuView("list");
                setNameError(null);
              }}
            >
              ← Back
            </button>
            <div className={styles.newScriptForm}>
              <span className={styles.newScriptLabel}>
                Behavior name (e.g. CoinPickup)
              </span>
              <input
                ref={nameInputRef}
                className={styles.newScriptInput}
                value={draftName}
                aria-label="New behavior name"
                onChange={(e) => {
                  setDraftName(e.target.value);
                  setNameError(null);
                }}
                onKeyDown={(e) => {
                  if (e.key === "Enter") {
                    e.preventDefault();
                    void createNewScript();
                  }
                }}
              />
              {nameError ? <p className={styles.formError}>{nameError}</p> : null}
              <div className={styles.newScriptActions}>
                <button
                  type="button"
                  className={styles.newScriptBtn}
                  onClick={() => {
                    setMenuView("list");
                    setNameError(null);
                  }}
                >
                  Cancel
                </button>
                <button
                  type="button"
                  className={`${styles.newScriptBtn} ${styles.newScriptBtnPrimary}`}
                  onClick={() => void createNewScript()}
                >
                  Create
                </button>
              </div>
            </div>
          </>
        ) : null}
        {menuView === "list" ? (
          <>
            <div
              className={styles.dropdownItem}
              style={{ fontWeight: 600 }}
              role="menuitem"
              onClick={(e) => {
                e.stopPropagation();
                setDraftName("NewBehavior");
                setNameError(null);
                setMenuView("newScriptName");
              }}
            >
              <FaPlus style={{ width: 12, height: 12, flexShrink: 0 }} />
              New script…
            </div>
            <div className={styles.dropdownDivider} />
            {behaviors.length === 0 ? (
              <div className={styles.dropdownItem} style={{ opacity: 0.5 }}>
                No behaviors registered
              </div>
            ) : (
              behaviors.map((b) => (
                <div
                  key={b}
                  className={styles.dropdownItem}
                  role="menuitem"
                  onClick={(e) => {
                    e.stopPropagation();
                    void engine.runtime.addScript(entityId, b);
                    closeMenu();
                  }}
                >
                  <FaCode style={{ width: 12, height: 12, flexShrink: 0 }} />
                  {b}
                </div>
              ))
            )}
          </>
        ) : null}
      </div>,
      document.body,
    );

  return (
    <div ref={anchorRef} style={{ position: "relative", padding: "8px" }}>
      <button type="button" className={styles.addComponentBtn} onClick={toggleOpen}>
        <FaPlus style={{ width: 10, height: 10 }} /> Add Component
      </button>
      {menuPortal}
    </div>
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
  onOpenLuaFile,
}: {
  entity: EngineEntity | null;
  onOpenLuaFile?: (path: string, content?: string) => void;
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
        <AddComponentButton
          entityId={entityId}
          onOpenLuaFile={onOpenLuaFile ?? (() => {})}
        />
      </div>
    </div>
  );
}
