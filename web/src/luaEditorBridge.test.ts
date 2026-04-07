import { afterEach, describe, expect, it } from "vitest";
import { captureEngineBridgePosts } from "./test/engineBridgeTestUtils";
import {
  installEngineScriptBridgeShim,
  isEngineScriptBridgeAvailable,
  reloadBehaviors,
  reloadScripts,
  resetEngineScriptBridgeForTests,
  setGamePaused,
  startSimulation,
} from "./luaEditorBridge";

afterEach(() => {
  resetEngineScriptBridgeForTests();
  delete window.webkit;
});

describe("isEngineScriptBridgeAvailable", () => {
  it("is true when WK webkit engineScript handler exists", () => {
    window.webkit = {
      messageHandlers: { engineScript: { postMessage: () => {} } },
    } as Window["webkit"];
    installEngineScriptBridgeShim();
    expect(isEngineScriptBridgeAvailable()).toBe(true);
  });

  it("is false in a plain browser environment", () => {
    installEngineScriptBridgeShim();
    expect(isEngineScriptBridgeAvailable()).toBe(false);
  });
});

describe("engine script IPC", () => {
  it("reloadScripts sends restartGame op to native", async () => {
    const { posted, deliver } = captureEngineBridgePosts();
    const p = reloadScripts();
    expect(posted.some((m) => m.op === "restartGame")).toBe(true);
    const msg = posted.find((m) => m.op === "restartGame");
    deliver({ requestId: String(msg?.requestId ?? ""), ok: true });
    await p;
  });

  it("reloadBehaviors sends reloadBehaviors op to native", async () => {
    const { posted, deliver } = captureEngineBridgePosts();
    const p = reloadBehaviors();
    expect(posted.some((m) => m.op === "reloadBehaviors")).toBe(true);
    const msg = posted.find((m) => m.op === "reloadBehaviors");
    deliver({ requestId: String(msg?.requestId ?? ""), ok: true });
    await p;
  });

  it("startSimulation sends op and captureEditorSnapshot (default true)", async () => {
    const { posted, deliver } = captureEngineBridgePosts();
    const p = startSimulation();
    expect(posted.some((m) => m.op === "startSimulation")).toBe(true);
    const msg = posted.find((m) => m.op === "startSimulation");
    expect(msg?.captureEditorSnapshot).toBe(true);
    deliver({ requestId: String(msg?.requestId ?? ""), ok: true });
    await p;
  });

  it("startSimulation(false) omits saving editor snapshot (resume from pause)", async () => {
    const { posted, deliver } = captureEngineBridgePosts();
    const p = startSimulation(false);
    const msg = posted.find((m) => m.op === "startSimulation");
    expect(msg?.captureEditorSnapshot).toBe(false);
    deliver({ requestId: String(msg?.requestId ?? ""), ok: true });
    await p;
  });

  it("setGamePaused sends paused flag", async () => {
    const { posted, deliver } = captureEngineBridgePosts();
    const p = setGamePaused(true);
    const msg = posted.find((m) => m.op === "setPaused");
    expect(msg?.paused).toBe(true);
    deliver({ requestId: String(msg?.requestId ?? ""), ok: true });
    await p;
  });

});
