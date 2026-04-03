import { Dockable } from "@danfessler/react-dockable";
import "@danfessler/react-dockable/style.css";
import { useState } from "react";
import { useGameRectBridge } from "./useGameRectBridge";
import "./App.css";

export default function App() {
  const [gameSurface, setGameSurface] = useState<HTMLDivElement | null>(null);
  useGameRectBridge(gameSurface);

  return (
    <div className="hud-shell">
      <div className="hud-dock">
        <Dockable.Root orientation="row" theme="darker" gap={6} radius={6}>
          <Dockable.Panel size={1}>
            <Dockable.Tab id="left-tools" name="Tools">
              <div className="panel-placeholder">
                <p>
                  <strong>Tools</strong>
                </p>
                <p className="muted">
                  From{" "}
                  <a
                    href="https://github.com/DanFessler/react-dockable"
                    target="_blank"
                    rel="noreferrer"
                  >
                    react-dockable
                  </a>
                  . Drag tabs to dock / undock.
                </p>
              </div>
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
            <Dockable.Tab id="right-inspector" name="Inspector">
              <div className="panel-placeholder">
                <p>
                  <strong>Inspector</strong>
                </p>
                <p className="muted">HUD / debug — opaque like side columns.</p>
              </div>
            </Dockable.Tab>
          </Dockable.Panel>
        </Dockable.Root>
      </div>
    </div>
  );
}
