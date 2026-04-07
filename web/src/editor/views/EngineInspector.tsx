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
  BehaviorComponent,
  BehaviorPropertyField,
  EngineEntity,
} from "../../engineBridge";
import { engine } from "../../engineProxy";
import {
  listProjectDir,
  readProjectFile,
  type ProjectDirEntry,
} from "../../projectFileBridge";
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

const IMAGE_PATH_RE = /\.(png|jpe?g|gif|bmp|webp)$/i;

function imageMimeForPath(p: string): string {
  const l = p.toLowerCase();
  if (l.endsWith(".png")) return "image/png";
  if (l.endsWith(".jpg") || l.endsWith(".jpeg")) return "image/jpeg";
  if (l.endsWith(".gif")) return "image/gif";
  if (l.endsWith(".webp")) return "image/webp";
  if (l.endsWith(".bmp")) return "image/bmp";
  return "image/png";
}

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

function parseBehaviorVector(value: unknown): [number, number] {
  const ch = (raw: unknown, fallback: number) => {
    const x = Number(raw);
    return Number.isFinite(x) ? x : fallback;
  };
  if (Array.isArray(value)) return [ch(value[0], 0), ch(value[1], 0)];
  if (value && typeof value === "object") {
    const obj = value as { x?: unknown; y?: unknown };
    return [ch(obj.x, 0), ch(obj.y, 0)];
  }
  return [0, 0];
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
  behaviorKind,
  hasEditorPair,
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
  sortableActivatorRef?: (element: HTMLElement | null) => void;
  sortableListeners?: DraggableSyntheticListeners;
  behaviorKind?: "engine" | "user";
  hasEditorPair?: boolean;
  children: ReactNode;
}) {
  return (
    <div
      className={styles.behaviorContainer}
      style={{ boxShadow: "0 -1px 0 rgba(0, 0, 0, 0.1)" }}
      data-behavior-kind={behaviorKind}
      data-has-editor-pair={hasEditorPair != null ? String(hasEditorPair) : undefined}
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
        {behaviorKind === "engine" ? (
          <span className={styles.engineBadge} title="Engine behavior">E</span>
        ) : null}
        {hasEditorPair ? (
          <span className={styles.editorPairBadge} title="Has editor behavior">✎</span>
        ) : null}
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

type InspectorFieldSpec = {
  key: string;
  label: string;
  field: BehaviorPropertyField;
  value: unknown;
  onCommit: (value: unknown) => void;
};

function BehaviorBoolRow({
  label,
  value,
  onCommit,
}: {
  label: string;
  value: unknown;
  onCommit: (value: unknown) => void;
}) {
  const checked = value === true || value === "true";
  return (
    <>
      <span className={styles.fieldLabel}>{label}</span>
      <label
        className={`${styles.fieldInput} ${styles.fieldSpan2}`}
        style={{ display: "flex", alignItems: "center", gap: 8 }}
      >
        <input
          type="checkbox"
          checked={checked}
          onChange={(e) => onCommit(e.target.checked)}
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
  label,
  field,
  value,
  onCommit,
}: {
  label: string;
  field: BehaviorPropertyField;
  value: unknown;
  onCommit: (value: unknown) => void;
}) {
  const [r, g, b, a255] = parseBehaviorColor(value);
  const fromProps: RgbaColor = useMemo(
    () => ({ r, g, b, a: a255 / 255 }),
    [r, g, b, a255],
  );

  const [open, setOpen] = useState(false);
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
      onCommit([
        Math.max(0, Math.min(255, Math.round(c.r))),
        Math.max(0, Math.min(255, Math.round(c.g))),
        Math.max(0, Math.min(255, Math.round(c.b))),
        Math.max(0, Math.min(255, Math.round(c.a * 255))),
      ]);
    },
    [onCommit],
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
        {label}
      </span>
      <div className={styles.colorPopoverWrap} ref={wrapRef}>
        <button
          type="button"
          className={styles.colorSwatchTrigger}
          aria-label={`${label} — open color picker`}
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
  label,
  field,
  value,
  onCommit,
}: {
  label: string;
  field: BehaviorPropertyField;
  value: unknown;
  onCommit: (value: unknown) => void;
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
    onCommit(v);
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
    onCommit(x);
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
        <span className={styles.fieldLabel} title={label}>
          {label}
        </span>
        <input
          type="range"
          className={styles.fieldRange}
          min={min}
          max={max}
          step={field.type === "integer" ? 1 : "any"}
          value={safe}
          aria-label={`${label} slider`}
          onChange={(e) => pushLive(parseFloat(e.target.value))}
        />
        <input
          type="number"
          className={styles.fieldInput}
          value={local}
          aria-label={`${label} exact value`}
          onChange={(e) => setLocal(e.target.value)}
          onBlur={commit}
          onKeyDown={(e) => e.key === "Enter" && commit()}
        />
      </>
    );
  }

  return (
    <>
      <span className={styles.fieldLabel} title={label}>
        {label}
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

function BehaviorVectorRow({
  label,
  value,
  onCommit,
}: {
  label: string;
  value: unknown;
  onCommit: (value: unknown) => void;
}) {
  const [x, y] = parseBehaviorVector(value);
  const [localX, setLocalX] = useState(String(x));
  const [localY, setLocalY] = useState(String(y));
  useEffect(() => setLocalX(String(x)), [x]);
  useEffect(() => setLocalY(String(y)), [y]);

  const commit = (axis: "x" | "y") => {
    const nextX = axis === "x" ? parseFloat(localX) : x;
    const nextY = axis === "y" ? parseFloat(localY) : y;
    if (!Number.isFinite(nextX) || !Number.isFinite(nextY)) {
      setLocalX(String(x));
      setLocalY(String(y));
      return;
    }
    if (nextX === x && nextY === y) {
      setLocalX(String(x));
      setLocalY(String(y));
      return;
    }
    onCommit([nextX, nextY]);
  };

  return (
    <>
      <span className={styles.fieldLabel} title={label}>
        {label}
      </span>
      <input
        className={styles.fieldInput}
        type="number"
        value={localX}
        onChange={(e) => setLocalX(e.target.value)}
        onBlur={() => commit("x")}
        onKeyDown={(e) => e.key === "Enter" && commit("x")}
        aria-label={`${label} X`}
      />
      <input
        className={styles.fieldInput}
        type="number"
        value={localY}
        onChange={(e) => setLocalY(e.target.value)}
        onBlur={() => commit("y")}
        onKeyDown={(e) => e.key === "Enter" && commit("y")}
        aria-label={`${label} Y`}
      />
    </>
  );
}

function BehaviorEnumRow({
  label,
  field,
  value,
  onCommit,
}: {
  label: string;
  field: BehaviorPropertyField;
  value: unknown;
  onCommit: (value: unknown) => void;
}) {
  const opts = field.enumOptions!;
  const v = typeof value === "string" ? value : String(value ?? opts[0]);
  return (
    <>
      <span className={styles.fieldLabel}>{label}</span>
      <select
        className={`${styles.fieldInput} ${styles.fieldSpan2}`}
        value={v}
        onChange={(e) => onCommit(e.target.value)}
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

function AssetPickerModal({
  onClose,
  onPick,
}: {
  onClose: () => void;
  onPick: (relativePath: string) => void;
}) {
  const [segments, setSegments] = useState<string[]>([]);
  const [entries, setEntries] = useState<ProjectDirEntry[]>([]);
  const [loadErr, setLoadErr] = useState<string | null>(null);

  useEffect(() => {
    let cancelled = false;
    const dir = segments.join("/");
    setLoadErr(null);
    void listProjectDir(dir)
      .then((list) => {
        if (!cancelled) setEntries(list);
      })
      .catch((e) => {
        if (!cancelled)
          setLoadErr(e instanceof Error ? e.message : String(e));
      });
    return () => {
      cancelled = true;
    };
  }, [segments]);

  useEffect(() => {
    const onKey = (e: KeyboardEvent) => {
      if (e.key === "Escape") onClose();
    };
    window.addEventListener("keydown", onKey);
    return () => window.removeEventListener("keydown", onKey);
  }, [onClose]);

  return createPortal(
    <div
      className={styles.assetPickerBackdrop}
      role="presentation"
      onMouseDown={(e) => {
        if (e.target === e.currentTarget) onClose();
      }}
    >
      <div
        className={styles.assetPickerPanel}
        role="dialog"
        aria-label="Choose image asset"
        onMouseDown={(e) => e.stopPropagation()}
      >
        <div className={styles.assetPickerHeader}>
          <span>Pick image</span>
          <button type="button" className={styles.assetBrowseBtn} onClick={onClose}>
            Close
          </button>
        </div>
        <div className={styles.assetPickerCrumbs}>
          <button
            type="button"
            className={styles.assetPickerCrumb}
            onClick={() => setSegments([])}
          >
            project
          </button>
          {segments.map((s, i) => (
            <span key={`${s}-${i}`}>
              <span aria-hidden> / </span>
              <button
                type="button"
                className={styles.assetPickerCrumb}
                onClick={() => setSegments(segments.slice(0, i + 1))}
              >
                {s}
              </button>
            </span>
          ))}
        </div>
        {loadErr ? <div className={styles.assetPickerErr}>{loadErr}</div> : null}
        <div className={styles.assetPickerList}>
          {entries
            .filter((e) => e.isDirectory || IMAGE_PATH_RE.test(e.name))
            .map((e) => (
              <div
                key={e.path}
                className={styles.assetPickerRow}
                onDoubleClick={() => {
                  if (e.isDirectory) {
                    setSegments([...segments, e.name]);
                  } else if (IMAGE_PATH_RE.test(e.name)) {
                    onPick(e.path);
                    onClose();
                  }
                }}
              >
                {e.isDirectory ? "📁" : "🖼"}
                <span>{e.name}</span>
              </div>
            ))}
        </div>
      </div>
    </div>,
    document.body,
  );
}

function BehaviorAssetRow({
  label,
  field,
  value,
  onCommit,
}: {
  label: string;
  field: BehaviorPropertyField;
  value: unknown;
  onCommit: (value: unknown) => void;
}) {
  const v =
    typeof value === "string" ? value : value == null ? "" : String(value);
  const [local, setLocal] = useState(v);
  const [pickerOpen, setPickerOpen] = useState(false);
  const [thumbDataUrl, setThumbDataUrl] = useState<string | null>(null);

  useEffect(() => {
    setLocal(
      typeof value === "string" ? value : value == null ? "" : String(value),
    );
  }, [value, field.name]);

  useEffect(() => {
    let cancelled = false;
    const p = local.trim();
    if (!p || !IMAGE_PATH_RE.test(p)) {
      setThumbDataUrl(null);
      return;
    }
    void readProjectFile(p, { encoding: "base64" })
      .then((b64) => {
        if (cancelled || !b64) return;
        setThumbDataUrl(`data:${imageMimeForPath(p)};base64,${b64}`);
      })
      .catch(() => {
        if (!cancelled) setThumbDataUrl(null);
      });
    return () => {
      cancelled = true;
    };
  }, [local]);

  const commit = () => {
    if (local !== v) onCommit(local);
  };

  return (
    <>
      <span className={styles.fieldLabel} title={label}>
        {label}
      </span>
      <div className={`${styles.fieldSpan2} ${styles.assetFieldWrap}`}>
        <button
          type="button"
          className={styles.assetThumbButton}
          aria-label={`${label} - choose image`}
          onClick={() => setPickerOpen(true)}
        >
          {thumbDataUrl ? (
            <img className={styles.assetThumb} src={thumbDataUrl} alt="" />
          ) : (
            <div className={styles.assetThumbPlaceholder} aria-hidden />
          )}
        </button>
        <input
          className={`${styles.fieldInput} ${styles.assetPathInput}`}
          type="text"
          value={local}
          onChange={(e) => setLocal(e.target.value)}
          onBlur={commit}
          onKeyDown={(e) => e.key === "Enter" && commit()}
          aria-label={label}
        />
      </div>
      {pickerOpen ? (
        <AssetPickerModal
          onClose={() => setPickerOpen(false)}
          onPick={(path) => {
            setLocal(path);
            onCommit(path);
            setPickerOpen(false);
          }}
        />
      ) : null}
    </>
  );
}

function BehaviorStringRow({
  label,
  field,
  value,
  onCommit,
}: {
  label: string;
  field: BehaviorPropertyField;
  value: unknown;
  onCommit: (value: unknown) => void;
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
    if (local !== v) onCommit(local);
  };
  return (
    <>
      <span className={styles.fieldLabel}>{label}</span>
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
  label,
  field,
  value,
  onCommit,
}: {
  label: string;
  field: BehaviorPropertyField;
  value: unknown;
  onCommit: (value: unknown) => void;
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
        onCommit(JSON.parse(local) as unknown);
      } catch {
        setLocal(raw);
      }
      return;
    }
    onCommit(local);
  };
  return (
    <>
      <span className={styles.fieldLabel}>{label}</span>
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

function BehaviorPropertyRow({ spec }: { spec: InspectorFieldSpec }) {
  const { label, field, value, onCommit } = spec;
  if (field.type === "boolean")
    return (
      <BehaviorBoolRow
        label={label}
        value={value}
        onCommit={onCommit}
      />
    );
  if (field.type === "number" || field.type === "integer")
    return (
      <BehaviorNumberRow
        label={label}
        field={field}
        value={value}
        onCommit={onCommit}
      />
    );
  if (field.type === "vector")
    return <BehaviorVectorRow label={label} value={value} onCommit={onCommit} />;
  if (field.type === "color")
    return (
      <BehaviorColorRow
        label={label}
        field={field}
        value={value}
        onCommit={onCommit}
      />
    );
  if (
    field.type === "enum" &&
    field.enumOptions &&
    field.enumOptions.length > 0
  )
    return (
      <BehaviorEnumRow
        label={label}
        field={field}
        value={value}
        onCommit={onCommit}
      />
    );
  if (field.type === "asset")
    return (
      <BehaviorAssetRow
        label={label}
        field={field}
        value={value}
        onCommit={onCommit}
      />
    );
  if (field.type === "string")
    return (
      <BehaviorStringRow
        label={label}
        field={field}
        value={value}
        onCommit={onCommit}
      />
    );
  return (
    <BehaviorJsonRow
      label={label}
      field={field}
      value={value}
      onCommit={onCommit}
    />
  );
}

function BehaviorPropertyEditor({
  entityId,
  behaviorIndex,
  comp,
}: {
  entityId: number;
  behaviorIndex: number;
  comp: BehaviorComponent;
}) {
  const fields = comp.propertySchema;
  const vals = comp.propertyValues ?? {};
  if (!fields?.length) return null;
  const specs: InspectorFieldSpec[] = fields.map((field) => ({
    key: field.name,
    label: field.name,
    field,
    value: vals[field.name] !== undefined ? vals[field.name] : field.default,
    onCommit: (value: unknown) =>
      engine.runtime.setBehaviorProperty(
        entityId,
        behaviorIndex,
        field.name,
        value,
      ),
  }));

  return (
    <>
      {specs.map((spec) => (
        <BehaviorPropertyRow
          key={spec.key}
          spec={spec}
        />
      ))}
    </>
  );
}

function BehaviorList({
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

  const behaviorEntries = entity.components
    .map((c, i) => ({ comp: c, idx: i }))
    .filter((x): x is { comp: BehaviorComponent; idx: number } => x.comp.type === "Behavior");

  const sensors = useSensors(
    useSensor(PointerSensor, { activationConstraint: { distance: 10 } }),
  );

  const behaviorIds = behaviorEntries.map((_, i) => `behavior-${i}`);

  function handleDragEnd(event: DragEndEvent) {
    const { active, over } = event;
    if (!over) return;
    if (active.id !== over.id) {
      const fromIdx = behaviorIds.indexOf(String(active.id));
      const toIdx = behaviorIds.indexOf(String(over.id));
      if (fromIdx >= 0 && toIdx >= 0) {
        const fromBehIdx = behaviorEntries[fromIdx].idx;
        const toBehIdx = behaviorEntries[toIdx].idx;
        engine.runtime.reorderScript(entityId, fromBehIdx, toBehIdx);
      }
    }
    setActiveChildren(null);
  }

  if (behaviorEntries.length === 0) return null;

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
      <SortableContext items={behaviorIds} strategy={verticalListSortingStrategy}>
        {behaviorEntries.map((entry, i) => {
          const comp = entry.comp;
          const behIdx = entry.idx;
          const isEngine = comp.isNative;
          const key = `behavior:${i}:${comp.name}`;
          const icon = isEngine
            ? <PiBoundingBoxFill style={{ width: "14px", height: "14px" }} />
            : <FaCode style={{ width: "14px", height: "14px" }} />;

          const propertyBody =
            comp.propertySchema && comp.propertySchema.length > 0 ? (
              <BehaviorPropertyEditor
                entityId={entityId}
                behaviorIndex={behIdx}
                comp={comp}
              />
            ) : (
              <span
                className={styles.fieldLabel}
                style={{ gridColumn: "1 / -1" }}
              >
                {isEngine
                  ? "Engine behavior"
                  : <>Luau behavior (add{" "}
                    <code>properties = defineProperties {"{ ... }"}</code> to expose
                    fields)</>}
              </span>
            );
          const dragOverlay = (
            <CollapsibleBlock
              name={comp.name}
              icon={icon}
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
              key={behaviorIds[i]}
              id={behaviorIds[i]}
              data={{ type: "behavior" }}
              dragOverlay={dragOverlay}
            >
              {(handle) => (
                <CollapsibleBlock
                  name={comp.name}
                  icon={icon}
                  canDisable={false}
                  active={true}
                  isCollapsed={collapseMap[key] ?? false}
                  onFold={(c) => onFold(key, c)}
                  onRemove={isEngine ? undefined : () => engine.runtime.removeScript(entityId, behIdx)}
                  sortableActivatorRef={handle.setActivatorNodeRef}
                  sortableListeners={handle.listeners}
                  behaviorKind={isEngine ? "engine" : "user"}
                  hasEditorPair={comp.hasEditorPair}
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
    if (entity) {
      entity.components.forEach((c, i) => {
        if (c.type === "Behavior") {
          parts.push(`behavior:${i}:${c.name}`);
        }
      });
    }
    return parts.join("\0");
  }, [entity]);
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
        <BehaviorList
          entity={entity}
          collapseMap={collapseMap}
          onFold={handleFold}
        />
      </div>
    </div>
  );
}
