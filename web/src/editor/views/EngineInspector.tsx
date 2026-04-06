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
import { RgbaColorPicker } from "react-colorful";
import type { RgbaColor } from "react-colorful";
import type {
  BehaviorPropertyField,
  EngineEntity,
  ScriptComponent,
  TransformComponent,
} from "../../engineBridge";
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
import type { DraggableSyntheticListeners } from "@dnd-kit/core";
import {
  SortableContext,
  verticalListSortingStrategy,
} from "@dnd-kit/sortable";
import { restrictToVerticalAxis } from "@dnd-kit/modifiers";
import SortableItem from "../components/SortableItem";
import styles from "./Inspector.module.css";

function parseBehaviorColor(value: unknown): [number, number, number, number] {
  if (!Array.isArray(value) || value.length < 3) return [255, 255, 255, 255];
  const ch = (i: number, fallback: number) => {
    const x = Number(value[i]);
    return Number.isFinite(x)
      ? Math.max(0, Math.min(255, Math.round(x)))
      : fallback;
  };
  const a = value.length >= 4 ? ch(3, 255) : 255;
  return [ch(0, 255), ch(1, 255), ch(2, 255), a];
}

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
  sortableActivatorRef,
  sortableListeners,
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
  /** When set with sortableListeners, dnd-kit drag only starts from this header (not property fields). */
  sortableActivatorRef?: (element: HTMLElement | null) => void;
  sortableListeners?: DraggableSyntheticListeners;
  children: ReactNode;
}) {
  return (
    <div
      className={styles.behaviorContainer}
      style={{ boxShadow: "0 -1px 0 rgba(0, 0, 0, 0.1)" }}
    >
      <div
        ref={sortableActivatorRef ?? undefined}
        {...(sortableListeners ?? {})}
        className={`${styles.behaviorHeader}${sortableListeners ? ` ${styles.behaviorHeaderDraggable}` : ""}`}
        onClick={() => onFold(!isCollapsed)}
      >
        <input
          type="checkbox"
          checked={active}
          disabled={!canDisable}
          aria-label={`${name} active`}
          onChange={() => onToggleActive?.()}
          onPointerDown={(e) => e.stopPropagation()}
          onClick={(e) => e.stopPropagation()}
        />
        {icon}
        <div>{name}</div>
        <div className={styles.spacer} />
        {onRemove ? (
          <button
            type="button"
            className={styles.removeBtn}
            aria-label={`Remove ${name}`}
            onPointerDown={(e) => e.stopPropagation()}
            onClick={(e) => {
              e.stopPropagation();
              onRemove();
            }}
          >
            <FaTimes />
          </button>
        ) : null}
        <div className={styles.arrowContainer}>{isCollapsed ? "▶" : "▼"}</div>
      </div>
      {!isCollapsed ? (
        <div className={styles.behaviorContent}>{children}</div>
      ) : null}
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

function BehaviorBoolRow({
  entityId,
  scriptIndex,
  field,
  value,
}: {
  entityId: number;
  scriptIndex: number;
  field: BehaviorPropertyField;
  value: unknown;
}) {
  const checked = value === true || value === "true";
  return (
    <>
      <span className={styles.fieldLabel}>{field.name}</span>
      <label
        className={`${styles.fieldInput} ${styles.fieldSpan2}`}
        style={{ display: "flex", alignItems: "center", gap: 8 }}
      >
        <input
          type="checkbox"
          checked={checked}
          onChange={(e) => {
            void engine.runtime.setScriptProperty(
              entityId,
              scriptIndex,
              field.name,
              e.target.checked,
            );
          }}
        />
      </label>
    </>
  );
}

const COLOR_POPOVER_MARGIN = 8;
const COLOR_POPOVER_GAP = 6;

function clampColorPopoverPosition(
  trigger: DOMRect,
  panelW: number,
  panelH: number,
): { top: number; left: number } {
  const vw = window.innerWidth;
  const vh = window.innerHeight;
  const m = COLOR_POPOVER_MARGIN;
  let top = trigger.bottom + COLOR_POPOVER_GAP;
  let left = trigger.left;
  if (top + panelH > vh - m) top = trigger.top - panelH - COLOR_POPOVER_GAP;
  if (top < m) top = m;
  if (top + panelH > vh - m) top = Math.max(m, vh - m - panelH);
  if (left + panelW > vw - m) left = vw - m - panelW;
  if (left < m) left = m;
  return { top, left };
}

function BehaviorColorRow({
  entityId,
  scriptIndex,
  field,
  value,
}: {
  entityId: number;
  scriptIndex: number;
  field: BehaviorPropertyField;
  value: unknown;
}) {
  const [r, g, b, a255] = parseBehaviorColor(value);
  const fromProps: RgbaColor = useMemo(
    () => ({ r, g, b, a: a255 / 255 }),
    [r, g, b, a255],
  );

  const [open, setOpen] = useState(false);
  /** While the popover is open, drive the picker from local state only so bridge round-trips don't fight the drag. */
  const [pickerColor, setPickerColor] = useState<RgbaColor>(fromProps);
  useEffect(() => {
    if (open) return;
    setPickerColor(fromProps);
  }, [fromProps, open]);

  const wrapRef = useRef<HTMLDivElement>(null);
  const panelRef = useRef<HTMLDivElement>(null);
  const [popoverPos, setPopoverPos] = useState({ top: 0, left: 0 });

  const updatePopoverPosition = useCallback(() => {
    const wrap = wrapRef.current;
    if (!wrap) return;
    const rect = wrap.getBoundingClientRect();
    const panel = panelRef.current;
    let pw = panel?.offsetWidth ?? 0;
    let ph = panel?.offsetHeight ?? 0;
    if (pw < 2 || ph < 2) {
      pw = 224;
      ph = 320;
    }
    setPopoverPos(clampColorPopoverPosition(rect, pw, ph));
  }, []);

  useLayoutEffect(() => {
    if (!open) return;
    updatePopoverPosition();
    const id = requestAnimationFrame(() => updatePopoverPosition());
    const panel = panelRef.current;
    const ro = new ResizeObserver(() => updatePopoverPosition());
    if (panel) ro.observe(panel);
    window.addEventListener("resize", updatePopoverPosition);
    window.addEventListener("scroll", updatePopoverPosition, true);
    return () => {
      cancelAnimationFrame(id);
      ro.disconnect();
      window.removeEventListener("resize", updatePopoverPosition);
      window.removeEventListener("scroll", updatePopoverPosition, true);
    };
  }, [open, updatePopoverPosition]);

  useEffect(() => {
    if (!open) return;
    const onDoc = (e: MouseEvent) => {
      const t = e.target as Node;
      if (wrapRef.current?.contains(t)) return;
      if (panelRef.current?.contains(t)) return;
      setOpen(false);
    };
    document.addEventListener("mousedown", onDoc);
    return () => document.removeEventListener("mousedown", onDoc);
  }, [open]);

  const pushColorToEngine = useCallback(
    (c: RgbaColor) => {
      void engine.runtime.setScriptProperty(entityId, scriptIndex, field.name, [
        Math.max(0, Math.min(255, Math.round(c.r))),
        Math.max(0, Math.min(255, Math.round(c.g))),
        Math.max(0, Math.min(255, Math.round(c.b))),
        Math.max(0, Math.min(255, Math.round(c.a * 255))),
      ]);
    },
    [entityId, scriptIndex, field.name],
  );

  const onPickerChange = useCallback(
    (c: RgbaColor) => {
      setPickerColor(c);
      pushColorToEngine(c);
    },
    [pushColorToEngine],
  );

  const togglePopover = useCallback(() => {
    setOpen((prev) => {
      if (!prev) setPickerColor(fromProps);
      return !prev;
    });
  }, [fromProps]);

  const swatch = open ? pickerColor : fromProps;
  const swatchR = Math.round(swatch.r);
  const swatchG = Math.round(swatch.g);
  const swatchB = Math.round(swatch.b);
  const swatchA = swatch.a;

  const popover =
    open && typeof document !== "undefined"
      ? createPortal(
          <div
            ref={panelRef}
            className={styles.colorPopoverPanel}
            role="dialog"
            aria-label={`${field.name} color`}
            style={{ top: popoverPos.top, left: popoverPos.left }}
            onMouseDown={(e) => e.stopPropagation()}
          >
            <RgbaColorPicker color={pickerColor} onChange={onPickerChange} />
          </div>,
          document.body,
        )
      : null;

  return (
    <>
      <span className={styles.fieldLabel} title={field.name}>
        {field.name}
      </span>
      <div className={styles.colorPopoverWrap} ref={wrapRef}>
        <button
          type="button"
          className={styles.colorSwatchTrigger}
          aria-label={`${field.name} — open color picker`}
          aria-expanded={open}
          aria-haspopup="dialog"
          onClick={togglePopover}
        >
          <span className={styles.colorSwatchChecker} aria-hidden />
          <span
            className={styles.colorSwatchFill}
            style={{
              background: `rgba(${swatchR},${swatchG},${swatchB},${swatchA})`,
            }}
            aria-hidden
          />
        </button>
        {popover}
      </div>
    </>
  );
}

function BehaviorNumberRow({
  entityId,
  scriptIndex,
  field,
  value,
}: {
  entityId: number;
  scriptIndex: number;
  field: BehaviorPropertyField;
  value: unknown;
}) {
  const n = typeof value === "number" ? value : Number(value);
  const base = Number.isFinite(n) ? n : 0;
  const [local, setLocal] = useState(String(base));
  useEffect(() => {
    const cur = typeof value === "number" ? value : Number(value);
    setLocal(String(Number.isFinite(cur) ? cur : 0));
  }, [value, field.name]);

  const commit = () => {
    let v = parseFloat(local);
    if (!Number.isFinite(v)) {
      setLocal(String(base));
      return;
    }
    if (field.min !== undefined) v = Math.max(field.min, v);
    if (field.max !== undefined) v = Math.min(field.max, v);
    if (field.type === "integer") v = Math.round(v);
    void engine.runtime.setScriptProperty(entityId, scriptIndex, field.name, v);
  };

  const useSlider =
    field.slider === true &&
    field.min !== undefined &&
    field.max !== undefined &&
    field.min < field.max;

  const pushLive = (v: number) => {
    let x = v;
    if (field.min !== undefined) x = Math.max(field.min, x);
    if (field.max !== undefined) x = Math.min(field.max, x);
    if (field.type === "integer") x = Math.round(x);
    setLocal(String(x));
    void engine.runtime.setScriptProperty(entityId, scriptIndex, field.name, x);
  };

  if (useSlider) {
    const min = field.min as number;
    const max = field.max as number;
    const parsed = parseFloat(local);
    const safe = Number.isFinite(parsed)
      ? Math.min(
          max,
          Math.max(min, field.type === "integer" ? Math.round(parsed) : parsed),
        )
      : min;
    return (
      <>
        <span className={styles.fieldLabel} title={field.name}>
          {field.name}
        </span>
        <input
          type="range"
          className={styles.fieldRange}
          min={min}
          max={max}
          step={field.type === "integer" ? 1 : "any"}
          value={safe}
          aria-label={`${field.name} slider`}
          onChange={(e) => pushLive(parseFloat(e.target.value))}
        />
        <input
          type="number"
          className={styles.fieldInput}
          value={local}
          aria-label={`${field.name} exact value`}
          onChange={(e) => setLocal(e.target.value)}
          onBlur={commit}
          onKeyDown={(e) => e.key === "Enter" && commit()}
        />
      </>
    );
  }

  return (
    <>
      <span className={styles.fieldLabel} title={field.name}>
        {field.name}
      </span>
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

function BehaviorEnumRow({
  entityId,
  scriptIndex,
  field,
  value,
}: {
  entityId: number;
  scriptIndex: number;
  field: BehaviorPropertyField;
  value: unknown;
}) {
  const opts = field.enumOptions!;
  const v = typeof value === "string" ? value : String(value ?? opts[0]);
  return (
    <>
      <span className={styles.fieldLabel}>{field.name}</span>
      <select
        className={`${styles.fieldInput} ${styles.fieldSpan2}`}
        value={v}
        onChange={(e) => {
          void engine.runtime.setScriptProperty(
            entityId,
            scriptIndex,
            field.name,
            e.target.value,
          );
        }}
      >
        {opts.map((opt) => (
          <option key={opt} value={opt}>
            {opt}
          </option>
        ))}
      </select>
    </>
  );
}

function BehaviorStringRow({
  entityId,
  scriptIndex,
  field,
  value,
}: {
  entityId: number;
  scriptIndex: number;
  field: BehaviorPropertyField;
  value: unknown;
}) {
  const v =
    typeof value === "string" ? value : value == null ? "" : String(value);
  const [local, setLocal] = useState(v);
  useEffect(() => {
    setLocal(
      typeof value === "string" ? value : value == null ? "" : String(value),
    );
  }, [value, field.name]);
  const commit = () => {
    if (local !== v)
      void engine.runtime.setScriptProperty(
        entityId,
        scriptIndex,
        field.name,
        local,
      );
  };
  return (
    <>
      <span className={styles.fieldLabel}>{field.name}</span>
      <input
        className={`${styles.fieldInput} ${styles.fieldSpan2}`}
        type="text"
        value={local}
        onChange={(e) => setLocal(e.target.value)}
        onBlur={commit}
        onKeyDown={(e) => e.key === "Enter" && commit()}
      />
    </>
  );
}

function BehaviorJsonRow({
  entityId,
  scriptIndex,
  field,
  value,
}: {
  entityId: number;
  scriptIndex: number;
  field: BehaviorPropertyField;
  value: unknown;
}) {
  const raw = (() => {
    try {
      return typeof value === "object"
        ? JSON.stringify(value)
        : String(value ?? "");
    } catch {
      return "";
    }
  })();
  const [local, setLocal] = useState(raw);
  useEffect(() => {
    try {
      setLocal(
        typeof value === "object" ? JSON.stringify(value) : String(value ?? ""),
      );
    } catch {
      setLocal("");
    }
  }, [value, field.name]);
  const commit = () => {
    const t = field.type;
    if (t === "object" || t === "vector") {
      try {
        const parsed = JSON.parse(local) as unknown;
        void engine.runtime.setScriptProperty(
          entityId,
          scriptIndex,
          field.name,
          parsed as never,
        );
      } catch {
        setLocal(raw);
      }
      return;
    }
    void engine.runtime.setScriptProperty(
      entityId,
      scriptIndex,
      field.name,
      local,
    );
  };
  return (
    <>
      <span className={styles.fieldLabel}>{field.name}</span>
      <textarea
        className={`${styles.fieldInput} ${styles.fieldSpan2}`}
        rows={3}
        value={local}
        onChange={(e) => setLocal(e.target.value)}
        onBlur={commit}
        style={{ fontFamily: "monospace", fontSize: "11px" }}
      />
    </>
  );
}

function BehaviorPropertyRow(props: {
  entityId: number;
  scriptIndex: number;
  field: BehaviorPropertyField;
  value: unknown;
}) {
  const { field } = props;
  if (field.type === "boolean") return <BehaviorBoolRow {...props} />;
  if (field.type === "number" || field.type === "integer")
    return <BehaviorNumberRow {...props} />;
  if (field.type === "color") return <BehaviorColorRow {...props} />;
  if (
    field.type === "enum" &&
    field.enumOptions &&
    field.enumOptions.length > 0
  )
    return <BehaviorEnumRow {...props} />;
  if (field.type === "string" || field.type === "asset")
    return <BehaviorStringRow {...props} />;
  return <BehaviorJsonRow {...props} />;
}

function BehaviorPropertyEditor({
  entityId,
  scriptIndex,
  comp,
}: {
  entityId: number;
  scriptIndex: number;
  comp: ScriptComponent;
}) {
  const fields = comp.propertySchema;
  const vals = comp.propertyValues ?? {};
  if (!fields?.length) return null;
  return (
    <>
      {fields.map((f) => (
        <BehaviorPropertyRow
          key={f.name}
          entityId={entityId}
          scriptIndex={scriptIndex}
          field={f}
          value={vals[f.name] !== undefined ? vals[f.name] : f.default}
        />
      ))}
    </>
  );
}

function TransformFields({
  t,
  entityId,
}: {
  t: TransformComponent;
  entityId: number;
}) {
  const set = (field: string) => (v: number) =>
    engine.runtime.setTransform(entityId, field, v);
  return (
    <>
      <EditableVec2
        label="Position"
        x={t.x}
        y={t.y}
        onCommitX={set("x")}
        onCommitY={set("y")}
      />
      <EditableFloat label="Angle" value={t.angle} onCommit={set("angle")} />
      <EditableVec2
        label="Velocity"
        x={t.vx}
        y={t.vy}
        onCommitX={set("vx")}
        onCommitY={set("vy")}
      />
    </>
  );
}

function ScriptList({
  entity,
  collapseMap,
  onFold,
}: {
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
          const propertyBody =
            sc.comp.propertySchema && sc.comp.propertySchema.length > 0 ? (
              <BehaviorPropertyEditor
                entityId={entityId}
                scriptIndex={i}
                comp={sc.comp}
              />
            ) : (
              <span
                className={styles.fieldLabel}
                style={{ gridColumn: "1 / -1" }}
              >
                Luau behavior (add{" "}
                <code>properties = defineProperties {"{ ... }"}</code> to expose
                fields)
              </span>
            );
          const dragOverlay = (
            <CollapsibleBlock
              name={sc.comp.behavior}
              icon={<FaCode style={{ width: "14px", height: "14px" }} />}
              canDisable={false}
              active={true}
              isCollapsed={false}
              onFold={() => {}}
              onRemove={undefined}
            >
              {propertyBody}
            </CollapsibleBlock>
          );
          return (
            <SortableItem
              key={scriptIds[i]}
              id={scriptIds[i]}
              data={{ type: "behavior" }}
              dragOverlay={dragOverlay}
            >
              {(handle) => (
                <CollapsibleBlock
                  name={sc.comp.behavior}
                  icon={<FaCode style={{ width: "14px", height: "14px" }} />}
                  canDisable={false}
                  active={true}
                  isCollapsed={collapseMap[key] ?? false}
                  onFold={(c) => onFold(key, c)}
                  onRemove={() => engine.runtime.removeScript(entityId, i)}
                  sortableActivatorRef={handle.setActivatorNodeRef}
                  sortableListeners={handle.listeners}
                >
                  {propertyBody}
                </CollapsibleBlock>
              )}
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
        <ScriptList
          entity={entity}
          collapseMap={collapseMap}
          onFold={handleFold}
        />
      </div>
    </div>
  );
}
