import { Dockable } from "@danfessler/react-dockable";
import "@danfessler/react-dockable/style.css";
import { useCallback, useEffect, useMemo, useRef, useState } from "react";
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
import "./App.css";

export default function App() {
  const [gameSurface, setGameSurface] = useState<HTMLDivElement | null>(null);
  const entities = useEngineEntities();
  const [selectedId, setSelectedId] = useState<string | null>(null);
  const [luaOpenFileRequest, setLuaOpenFileRequest] =
    useState<LuaEditorOpenRequest | null>(null);
  const luaOpenSeq = useRef(0);

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
    <div className="hud-shell">
      <Toolbar />
      <div className="hud-dock">
        <Dockable.Root orientation="row" theme="dark" gap={4} radius={4}>
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

          <Dockable.Panel size={5} orientation="column">
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
