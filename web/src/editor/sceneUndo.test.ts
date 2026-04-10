import { describe, expect, it } from "vitest";
import {
  sceneUndoHotkeyDecision,
  sceneUndoShouldYieldToTextControl,
} from "./sceneUndo";

describe("sceneUndo", () => {
  it("hotkey: invokes undo for meta+z when stopped and not in text control", () => {
    const d = sceneUndoHotkeyDecision(
      { metaKey: true, ctrlKey: false, key: "z", shiftKey: false },
      document.body,
      "stopped",
    );
    expect(d).toEqual({ action: "invoke", kind: "undo" });
  });

  it("hotkey: redo for shift+meta+z", () => {
    const d = sceneUndoHotkeyDecision(
      { metaKey: true, ctrlKey: false, key: "z", shiftKey: true },
      document.body,
      "stopped",
    );
    expect(d).toEqual({ action: "invoke", kind: "redo" });
  });

  it("hotkey: none while playing", () => {
    const d = sceneUndoHotkeyDecision(
      { metaKey: true, ctrlKey: false, key: "z", shiftKey: false },
      document.body,
      "playing",
    );
    expect(d).toEqual({ action: "none" });
  });

  it("hotkey: none when target is an input", () => {
    const input = document.createElement("input");
    const d = sceneUndoHotkeyDecision(
      { metaKey: true, ctrlKey: false, key: "z", shiftKey: false },
      input,
      "stopped",
    );
    expect(d).toEqual({ action: "none" });
  });

  it("shouldYieldToTextControl: true for textarea", () => {
    expect(sceneUndoShouldYieldToTextControl(document.createElement("textarea")))
      .toBe(true);
  });
});
