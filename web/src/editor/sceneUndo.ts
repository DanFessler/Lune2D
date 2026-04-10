import type { ToolbarSimState } from "../Toolbar";
import { engine } from "../engineProxy";

export type EditorSessionState = {
  canUndo: boolean;
  canRedo: boolean;
  /** Matches native `EngSimUiState`: 0 stopped, 1 playing, 2 paused */
  simUiState: number;
};

/** True if the event target is a text field or Monaco — scene undo should not steal ⌘Z. */
export function sceneUndoShouldYieldToTextControl(
  target: EventTarget | null,
): boolean {
  if (!(target instanceof HTMLElement)) return false;
  if (target.isContentEditable) return true;
  const tag = target.tagName;
  if (tag === "INPUT" || tag === "TEXTAREA" || tag === "SELECT") return true;
  if (target.closest(".monaco-editor")) return true;
  return false;
}

export async function fetchEditorSessionState(): Promise<EditorSessionState | null> {
  try {
    const res = await engine.editor.getSessionState();
    const r = res.result as Record<string, unknown> | undefined;
    if (!r || typeof r !== "object") return null;
    return {
      canUndo: r.canUndo === true,
      canRedo: r.canRedo === true,
      simUiState:
        typeof r.simUiState === "number" && Number.isFinite(r.simUiState)
          ? r.simUiState
          : 0,
    };
  } catch {
    return null;
  }
}

/** Explicit undo milestone: call at the end of an editor action (mouse-up, blur commit, etc). */
export function saveSceneUndoState(): void {
  if (!engine.editor?.saveUndoState) return;
  void engine.editor.saveUndoState();
}

export type SceneUndoHotkeyDecision =
  | { action: "none" }
  | { action: "invoke"; kind: "undo" | "redo" };

/**
 * Pure keyboard routing for scene undo/redo. Does not query native — use with
 * `fetchEditorSessionState` before invoking `engine.editor.undo` / `redo`.
 */
export function sceneUndoHotkeyDecision(
  e: Pick<KeyboardEvent, "metaKey" | "ctrlKey" | "key" | "shiftKey">,
  target: EventTarget | null,
  simUiState: ToolbarSimState,
): SceneUndoHotkeyDecision {
  if (!(e.metaKey || e.ctrlKey)) return { action: "none" };
  if (e.key.toLowerCase() !== "z") return { action: "none" };
  if (sceneUndoShouldYieldToTextControl(target)) return { action: "none" };
  if (simUiState === "playing") return { action: "none" };
  if (e.shiftKey) return { action: "invoke", kind: "redo" };
  return { action: "invoke", kind: "undo" };
}
