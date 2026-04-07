import { Dockable } from "@danfessler/react-dockable";
import type { LayoutNode } from "@danfessler/react-dockable";
import "@danfessler/react-dockable/style.css";
import { useCallback, useEffect, useMemo, useRef, useState } from "react";
import AssetBrowser from "./editor/views/AssetBrowser";
import { NewScriptDialog } from "./editor/NewScriptDialog";
import SceneHierarchy from "./editor/views/SceneHierarchy";
import EngineInspector from "./editor/views/EngineInspector";
import {
  inspectorTabActions,
  sceneHierarchyTabActions,
} from "./editor/tabActions";
import { LuaEditorPanel, type LuaEditorOpenRequest } from "./LuaEditorPanel";
import { Toolbar } from "./Toolbar";
import type { SceneOpResult } from "./generated/sceneOps";
import { listBehaviors } from "./luaEditorBridge";
import { engine, unwrapSceneNumber } from "./engineProxy";
import { useEngineEntities } from "./useEngineEntities";
import { useGameRectBridge } from "./useGameRectBridge";
import { dockableShellCssVars, type DockablePaletteName } from "./dockableShellTheme";
import {
  loadEditorDockLayout,
  saveEditorDockLayout,
} from "./editorLayoutStorage";
import "./App.css";

/** Single source for dock chrome: must match `dockableShellCssVars` + `Dockable.Root`. */
const DOCKABLE_CHROME: {
  theme: DockablePaletteName;
  gap: number;
  radius: number;
} = { theme: "dark", gap: 4, radius: 4 };

export default function App() {
  const [gameSurface, setGameSurface] = useState<HTMLDivElement | null>(null);
  const entities = useEngineEntities();
  const [selectedId, setSelectedId] = useState<string | null>(null);
  /** Skip pushing selection to native when it came from viewport/Luau (avoids races + echo). */
  const fromNativeSelectionRef = useRef(false);
  /** Tracks prior selectedId for sync effect (distinguish mount / StrictMode from real deselect). */
  const prevSelectedIdRef = useRef<string | null | undefined>(undefined);
  const [luaOpenFileRequest, setLuaOpenFileRequest] =
    useState<LuaEditorOpenRequest | null>(null);
  const luaOpenSeq = useRef(0);
  const [assetBrowserRefreshKey, setAssetBrowserRefreshKey] = useState(0);

  const [dockBootKey, setDockBootKey] = useState(0);
  const [restoredDockLayout, setRestoredDockLayout] = useState<
    LayoutNode[] | undefined
  >(undefined);
  const dockLayoutSaveOkRef = useRef(false);
  const dockSaveTimerRef = useRef<ReturnType<typeof setTimeout> | null>(null);

  useEffect(() => {
    let cancelled = false;
    void loadEditorDockLayout().then((layout) => {
      if (cancelled) return;
      if (layout) {
        setRestoredDockLayout(layout);
        setDockBootKey((k) => k + 1);
      }
      requestAnimationFrame(() => {
        if (!cancelled) dockLayoutSaveOkRef.current = true;
      });
    });
    return () => {
      cancelled = true;
    };
  }, []);

  useEffect(
    () => () => {
      if (dockSaveTimerRef.current) clearTimeout(dockSaveTimerRef.current);
    },
    [],
  );

  const onDockLayoutChange = useCallback((panels: LayoutNode[]) => {
    if (!dockLayoutSaveOkRef.current) return;
    if (dockSaveTimerRef.current) clearTimeout(dockSaveTimerRef.current);
    dockSaveTimerRef.current = setTimeout(() => {
      dockSaveTimerRef.current = null;
      void saveEditorDockLayout(panels);
    }, 400);
  }, []);

  const focusLuaFile = useCallback((path: string, content?: string) => {
    luaOpenSeq.current += 1;
    setLuaOpenFileRequest({
      path,
      content,
      id: luaOpenSeq.current,
    });
  }, []);

  const clearLuaOpenFileRequest = useCallback(() => {
    setLuaOpenFileRequest(null);
  }, []);

  const bumpAssetBrowserRefresh = useCallback(() => {
    setAssetBrowserRefreshKey((k) => k + 1);
  }, []);

  const loadSceneFromAssets = useCallback(async (relativePath: string) => {
    try {
      await engine.runtime.loadScene(relativePath);
    } catch (e) {
      console.error(e);
    }
  }, []);

  useGameRectBridge(gameSurface);

  const selected = useMemo(
    () => entities.find((e) => e.id === selectedId) ?? null,
    [entities, selectedId],
  );

  useEffect(() => {
    if (!selectedId) return;
    if (!entities.some((e) => e.id === selectedId)) setSelectedId(null);
  }, [entities, selectedId]);

  const [behaviorNames, setBehaviorNames] = useState<string[]>([]);
  useEffect(() => {
    void listBehaviors().then(setBehaviorNames);
  }, []);

  useEffect(() => {
    window.__engineSelectEntity = (id: number | string | null) => {
      fromNativeSelectionRef.current = true;
      if (id == null || id === "") setSelectedId(null);
      else setSelectedId(String(id));
    };
    return () => {
      if (window.__engineSelectEntity) delete window.__engineSelectEntity;
    };
  }, []);

  useEffect(() => {
    const bridge = window.__engineScriptBridge;
    if (!bridge) return;
    let cancelled = false;
    void bridge
      .call("editor.getSelectedEntityId", { args: [] })
      .then((res) => {
        if (cancelled) return;
        const raw = (res as { result?: unknown }).result;
        const id =
          typeof raw === "number" && Number.isFinite(raw)
            ? raw
            : typeof raw === "string" && raw !== ""
              ? Number(raw)
              : 0;
        if (id <= 0) return;
        fromNativeSelectionRef.current = true;
        setSelectedId((cur) => (cur == null ? String(id) : cur));
      })
      .catch(() => {});
    return () => {
      cancelled = true;
    };
  }, []);

  useEffect(() => {
    const bridge = window.__engineScriptBridge;
    if (!bridge) return;

    if (fromNativeSelectionRef.current) {
      fromNativeSelectionRef.current = false;
      prevSelectedIdRef.current = selectedId;
      return;
    }

    const prev = prevSelectedIdRef.current;
    prevSelectedIdRef.current = selectedId;

    // Avoid pushing null on initial mount / StrictMode duplicate pass; a late async RPC
    // would clear native selection after a viewport pick before React hydrates.
    if (selectedId == null && (prev === undefined || prev === null)) {
      return;
    }

    const args: unknown[] = selectedId == null ? [null] : [Number(selectedId)];
    void bridge.call("editor.setSelectedEntity", { args }).catch(() => {});
  }, [selectedId]);

  const [inspectorNewScriptOpen, setInspectorNewScriptOpen] = useState(false);

  useEffect(() => {
    if (!selectedId) setInspectorNewScriptOpen(false);
  }, [selectedId]);

  const refreshBehaviorNames = useCallback(() => {
    void listBehaviors().then(setBehaviorNames);
  }, []);

  const onNewEntity = useCallback(async () => {
    try {
      const res = await engine.runtime.spawn("Entity");
      const id = unwrapSceneNumber(res as unknown as SceneOpResult);
      setSelectedId(String(id));
    } catch (e) {
      console.error(e);
    }
  }, []);

  const hierarchyMenuActions = useMemo(
    () => sceneHierarchyTabActions({ onNewEntity }),
    [onNewEntity],
  );

  const inspectorMenuActions = useMemo(
    () =>
      inspectorTabActions({
        entity: selected,
        behaviorNames,
        onNewScript: () => setInspectorNewScriptOpen(true),
      }),
    [selected, behaviorNames],
  );

  return (
    <div
      className="hud-shell"
      style={dockableShellCssVars(
        DOCKABLE_CHROME.theme,
        DOCKABLE_CHROME.gap,
        DOCKABLE_CHROME.radius,
      )}
    >
      <Toolbar />
      <div className="hud-dock">
        <Dockable.Root
          key={dockBootKey}
          layout={restoredDockLayout}
          orientation="row"
          theme={DOCKABLE_CHROME.theme}
          gap={DOCKABLE_CHROME.gap}
          radius={DOCKABLE_CHROME.radius}
          onChange={onDockLayoutChange}
        >
          <Dockable.Panel size={2}>
            <Dockable.Tab
              id="left-hierarchy"
              name="Hierarchy"
              actions={hierarchyMenuActions}
            >
              <SceneHierarchy
                entities={entities}
                selectedId={selectedId}
                onSelect={setSelectedId}
              />
            </Dockable.Tab>
          </Dockable.Panel>

          <Dockable.Panel size={6} orientation="column">
            <Dockable.Window size={3}>
              <Dockable.Tab id="center-game" name="Game">
                <div className="game-dock-body">
                  <div
                    id="game-surface"
                    ref={setGameSurface}
                    aria-hidden="true"
                  />
                </div>
              </Dockable.Tab>
            </Dockable.Window>
            <Dockable.Window size={2}>
              <Dockable.Tab id="lua-editor" name="Lua">
                <LuaEditorPanel
                  openFileRequest={luaOpenFileRequest}
                  onConsumedOpenFileRequest={clearLuaOpenFileRequest}
                  onFilesMutated={bumpAssetBrowserRefresh}
                />
              </Dockable.Tab>
            </Dockable.Window>
            <Dockable.Window size={2}>
              <Dockable.Tab id="assets-browser" name="Assets">
                <AssetBrowser
                  refreshKey={assetBrowserRefreshKey}
                  onOpenEntry={(path) => focusLuaFile(path)}
                  onLoadScene={(path) => void loadSceneFromAssets(path)}
                />
              </Dockable.Tab>
            </Dockable.Window>
          </Dockable.Panel>

          <Dockable.Panel size={2}>
            <Dockable.Tab
              id="right-inspector"
              name="Inspector"
              actions={inspectorMenuActions}
            >
              <EngineInspector entity={selected} />
            </Dockable.Tab>
          </Dockable.Panel>
        </Dockable.Root>
      </div>
      <NewScriptDialog
        open={inspectorNewScriptOpen && selected !== null}
        onClose={() => setInspectorNewScriptOpen(false)}
        entityId={selected ? Number(selected.id) : 0}
        onOpenLuaFile={focusLuaFile}
        onScriptsChanged={refreshBehaviorNames}
      />
    </div>
  );
}
