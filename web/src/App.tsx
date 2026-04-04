import { Dockable } from "@danfessler/react-dockable";
import "@danfessler/react-dockable/style.css";
import { useCallback, useEffect, useMemo, useRef, useState } from "react";
import SceneHierarchy from "./editor/views/SceneHierarchy";
import EngineInspector from "./editor/views/EngineInspector";
import { LuaEditorPanel, type LuaEditorOpenRequest } from "./LuaEditorPanel";
import { Toolbar } from "./Toolbar";
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

  return (
    <div className="hud-shell">
      <Toolbar />
      <div className="hud-dock">
        <Dockable.Root orientation="row" theme="darker" gap={4} radius={4}>
          <Dockable.Panel size={2}>
            <Dockable.Tab id="left-hierarchy" name="Hierarchy">
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
            <Dockable.Tab id="right-inspector" name="Inspector">
              <EngineInspector entity={selected} onOpenLuaFile={focusLuaFile} />
            </Dockable.Tab>
          </Dockable.Panel>
        </Dockable.Root>
      </div>
    </div>
  );
}
