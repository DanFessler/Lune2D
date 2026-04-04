import { useCallback, useState } from "react";
import {
  reloadScripts,
  setGamePaused,
  startSimulation,
} from "./luaEditorBridge";
import "./Toolbar.css";

type SimState = "stopped" | "playing" | "paused";

export function Toolbar() {
  const [simState, setSimState] = useState<SimState>("stopped");
  const [error, setError] = useState<string | null>(null);

  const onPlay = useCallback(async () => {
    setError(null);
    try {
      const fromStopped = simState === "stopped";
      if (simState === "paused") {
        await setGamePaused(false);
      }
      await startSimulation(fromStopped);
      setSimState("playing");
    } catch (e) {
      setError(e instanceof Error ? e.message : String(e));
    }
  }, [simState]);

  const onPause = useCallback(async () => {
    setError(null);
    try {
      await setGamePaused(true);
      setSimState("paused");
    } catch (e) {
      setError(e instanceof Error ? e.message : String(e));
    }
  }, []);

  const onStop = useCallback(async () => {
    setError(null);
    try {
      await reloadScripts();
      setSimState("stopped");
    } catch (e) {
      setError(e instanceof Error ? e.message : String(e));
    }
  }, []);

  return (
    <div className="app-toolbar">
      <div className="toolbar-controls">
        <button
          type="button"
          className="toolbar-btn toolbar-play"
          onClick={() => void onPlay()}
          disabled={simState === "playing"}
          title="Play"
          aria-label="Play"
        >
          ▶
        </button>
        <button
          type="button"
          className="toolbar-btn toolbar-pause"
          onClick={() => void onPause()}
          disabled={simState !== "playing"}
          title="Pause"
          aria-label="Pause"
        >
          ⏸
        </button>
        <button
          type="button"
          className="toolbar-btn toolbar-stop"
          onClick={() => void onStop()}
          disabled={simState === "stopped"}
          title="Stop"
          aria-label="Stop"
        >
          ⏹
        </button>
      </div>
      {error && <span className="toolbar-error">{error}</span>}
    </div>
  );
}
