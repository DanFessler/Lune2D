import { afterEach, describe, expect, it } from "vitest";
import {
  EDITOR_DOCK_LAYOUT_VERSION,
  loadEditorDockLayout,
  saveEditorDockLayout,
} from "./editorLayoutStorage";
import {
  installEngineScriptBridgeShim,
  resetEngineScriptBridgeForTests,
} from "./luaEditorBridge";

afterEach(() => {
  resetEngineScriptBridgeForTests();
  localStorage.removeItem("lune2d-editor-dock-layout");
});

describe("editorLayoutStorage", () => {
  it("loads layout JSON from localStorage when bridge read fails", async () => {
    const payload = {
      version: EDITOR_DOCK_LAYOUT_VERSION,
      layout: [
        {
          type: "Panel" as const,
          id: "p0",
          size: 1,
          orientation: "row" as const,
          children: [
            {
              type: "Window" as const,
              id: "w0",
              size: 1,
              selected: "left-hierarchy",
              children: ["left-hierarchy"],
            },
            {
              type: "Window" as const,
              id: "w1",
              size: 1,
              selected: "center-game",
              children: ["center-game"],
            },
          ],
        },
      ],
    };
    localStorage.setItem("lune2d-editor-dock-layout", JSON.stringify(payload));

    installEngineScriptBridgeShim((msg) => {
      queueMicrotask(() =>
        window.__engineScriptBridge!.receive({
          requestId: String(msg.requestId),
          ok: false,
          error: "not found",
        }),
      );
    });

    const layout = await loadEditorDockLayout();
    expect(layout).not.toBeNull();
    expect(layout!.length).toBe(1);
  });

  it("rejects persisted layout with unknown tab ids", async () => {
    localStorage.setItem(
      "lune2d-editor-dock-layout",
      JSON.stringify({
        version: EDITOR_DOCK_LAYOUT_VERSION,
        layout: [
          {
            type: "Panel",
            id: "p0",
            size: 1,
            orientation: "row",
            children: [
              {
                type: "Window",
                id: "w0",
                size: 1,
                selected: "nope",
                children: ["nope"],
              },
            ],
          },
        ],
      }),
    );

    installEngineScriptBridgeShim((msg) => {
      queueMicrotask(() =>
        window.__engineScriptBridge!.receive({
          requestId: String(msg.requestId),
          ok: false,
          error: "not found",
        }),
      );
    });

    const layout = await loadEditorDockLayout();
    expect(layout).toBeNull();
  });

  it("writes via bridge when available", async () => {
    const written: { op?: string; content?: string }[] = [];
    installEngineScriptBridgeShim((msg) => {
      const rec = msg as Record<string, unknown>;
      const op = String(rec.op ?? "");
      if (op === "writeSharedSettingsFile")
        written.push({ op, content: String(rec.content ?? "") });
      queueMicrotask(() =>
        window.__engineScriptBridge!.receive({
          requestId: String(msg.requestId),
          ok: true,
        }),
      );
    });

    await saveEditorDockLayout([
      {
        type: "Panel",
        id: "p0",
        size: 1,
        orientation: "row",
        children: [
          {
            type: "Window",
            id: "w0",
            size: 1,
            selected: "left-hierarchy",
            children: ["left-hierarchy"],
          },
          {
            type: "Window",
            id: "w1",
            size: 1,
            selected: "center-game",
            children: ["center-game"],
          },
        ],
      },
    ]);

    expect(written.length).toBe(1);
    expect(written[0].content).toContain('"version":1');
    expect(written[0].content).toContain("left-hierarchy");
  });
});
