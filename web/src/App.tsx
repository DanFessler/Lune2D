import { Dockable } from "@danfessler/react-dockable";
import "@danfessler/react-dockable/style.css";
import { useEffect, useMemo, useState } from "react";
import { EntityListPanel } from "./EntityListPanel";
import { InspectorPanel } from "./InspectorPanel";
import { LuaEditorPanel } from "./LuaEditorPanel";
import { useEngineEntities } from "./useEngineEntities";
import { useGameRectBridge } from "./useGameRectBridge";
import "./App.css";

export default function App() {
  const [gameSurface, setGameSurface] = useState<HTMLDivElement | null>(null);
  const entities = useEngineEntities();
  const [selectedId, setSelectedId] = useState<string | null>(null);

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
      <div className="hud-dock">
        <Dockable.Root orientation="row" theme="darker" gap={6} radius={6}>
          <Dockable.Panel size={1}>
            <Dockable.Tab id="left-entities" name="Entities">
              <EntityListPanel
                entities={entities}
                selectedId={selectedId}
                onSelect={setSelectedId}
              />
            </Dockable.Tab>
          </Dockable.Panel>
          <Dockable.Panel size={2}>
            <Dockable.Tab id="center-game" name="Game">
              <div className="game-dock-body">
                <div
                  id="game-surface"
                  ref={setGameSurface}
                  aria-hidden="true"
                />
              </div>
            </Dockable.Tab>
          </Dockable.Panel>
          <Dockable.Panel size={1}>
            <Dockable.Tab id="lua-editor" name="Lua">
              <LuaEditorPanel />
            </Dockable.Tab>
          </Dockable.Panel>
          <Dockable.Panel size={1}>
            <Dockable.Tab id="right-inspector" name="Inspector">
              <InspectorPanel entity={selected} />
            </Dockable.Tab>
          </Dockable.Panel>
        </Dockable.Root>
      </div>
    </div>
  );
}
