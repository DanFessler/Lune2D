import type { LayoutNode } from "@danfessler/react-dockable";
import type { EngineScriptResponse } from "./luaEditorBridge";

type ReadSharedSettingsResponse = EngineScriptResponse & { content?: string };

/** Bump when dock tab set or serialized shape changes. */
export const EDITOR_DOCK_LAYOUT_VERSION = 1;

const REL_PATH = "editor-dock-layout.json";
const LOCALSTORAGE_KEY = "lune2d-editor-dock-layout";

/** Tab ids from `App.tsx` — rejected layouts fall back to defaults. */
const KNOWN_TAB_IDS = new Set([
  "left-hierarchy",
  "center-game",
  "lua-editor",
  "assets-browser",
  "right-inspector",
]);

type PersistedDockLayout = {
  version: number;
  layout: LayoutNode[];
};

function layoutLooksValid(nodes: LayoutNode[]): boolean {
  const walk = (node: LayoutNode): boolean => {
    if (node.type === "Window") {
      if (!Array.isArray(node.children) || node.children.length === 0)
        return false;
      for (const tabId of node.children) {
        if (typeof tabId !== "string" || !KNOWN_TAB_IDS.has(tabId))
          return false;
      }
      if (typeof node.selected !== "string" || !KNOWN_TAB_IDS.has(node.selected))
        return false;
      if (!node.children.includes(node.selected)) return false;
      return true;
    }
    if (node.type === "Panel") {
      if (!Array.isArray(node.children) || node.children.length === 0)
        return false;
      return node.children.every(walk);
    }
    return false;
  };
  return nodes.length > 0 && nodes.every(walk);
}

function parsePersisted(raw: unknown): LayoutNode[] | null {
  if (!raw || typeof raw !== "object") return null;
  const o = raw as PersistedDockLayout;
  if (o.version !== EDITOR_DOCK_LAYOUT_VERSION) return null;
  if (!Array.isArray(o.layout)) return null;
  return layoutLooksValid(o.layout) ? o.layout : null;
}

async function readViaBridge(): Promise<string | null> {
  const bridge = window.__engineScriptBridge;
  if (!bridge) return null;
  try {
    const res = await bridge.call<ReadSharedSettingsResponse>(
      "readSharedSettingsFile",
      { relPath: REL_PATH },
    );
    if (typeof res.content === "string") return res.content;
    return null;
  } catch {
    return null;
  }
}

async function writeViaBridge(json: string): Promise<boolean> {
  const bridge = window.__engineScriptBridge;
  if (!bridge) return false;
  try {
    await bridge.call("writeSharedSettingsFile", {
      relPath: REL_PATH,
      content: json,
    });
    return true;
  } catch {
    return false;
  }
}

function readLocalStorage(): LayoutNode[] | null {
  try {
    const raw = localStorage.getItem(LOCALSTORAGE_KEY);
    if (!raw) return null;
    return parsePersisted(JSON.parse(raw));
  } catch {
    return null;
  }
}

function writeLocalStorage(json: string): void {
  try {
    localStorage.setItem(LOCALSTORAGE_KEY, json);
  } catch {
    /* ignore quota / private mode */
  }
}

/** Load persisted dock layout from app support (desktop) or localStorage (browser dev). */
export async function loadEditorDockLayout(): Promise<LayoutNode[] | null> {
  const text = await readViaBridge();
  if (text !== null && text.length > 0) {
    try {
      return parsePersisted(JSON.parse(text));
    } catch {
      return null;
    }
  }
  return readLocalStorage();
}

/** Save dock layout to the shared store (all projects). */
export async function saveEditorDockLayout(layout: LayoutNode[]): Promise<void> {
  if (!layoutLooksValid(layout)) return;
  const payload: PersistedDockLayout = {
    version: EDITOR_DOCK_LAYOUT_VERSION,
    layout,
  };
  const json = JSON.stringify(payload);
  if (await writeViaBridge(json)) return;
  writeLocalStorage(json);
}
