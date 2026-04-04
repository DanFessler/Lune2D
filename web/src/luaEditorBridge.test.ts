import { afterEach, describe, expect, it } from "vitest";
import { captureEngineBridgePosts } from "./test/engineBridgeTestUtils";
import {
  installEngineScriptBridgeShim,
  isEngineScriptBridgeAvailable,
  listLuaFiles,
  reloadBehaviors,
  reloadScripts,
  resetEngineScriptBridgeForTests,
  setGamePaused,
  startSimulation,
  writeLuaFile,
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
  it("listLuaFiles resolves with files from native response", async () => {
    const { posted, deliver } = captureEngineBridgePosts();
    const p = listLuaFiles();
    expect(posted.some((m) => m.op === "listLua")).toBe(true);
    const rid = String(posted.find((m) => m.op === "listLua")?.requestId ?? "");
    deliver({
      requestId: rid,
      ok: true,
      files: [
        { path: "game.lua", content: "-- hello" },
        { path: "game/session.lua", content: "return {}" },
      ],
    });
    const files = await p;
    expect(files).toHaveLength(2);
    expect(files[0].path).toBe("game.lua");
  });

  it("writeLuaFile sends path and content", async () => {
    const { posted, deliver } = captureEngineBridgePosts();
    const p = writeLuaFile("game.lua", "x = 1");
    const msg = posted.find((m) => m.op === "writeLua");
    expect(msg?.path).toBe("game.lua");
    expect(msg?.content).toBe("x = 1");
    deliver({
      requestId: String(msg?.requestId ?? ""),
      ok: true,
    });
    await p;
  });

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

  it("writeLuaFile rejects when native responds with ok: false", async () => {
    const { posted, deliver } = captureEngineBridgePosts();
    const p = writeLuaFile("game.lua", "x=1");
    const msg = posted.find((m) => m.op === "writeLua");
    deliver({
      requestId: String(msg?.requestId ?? ""),
      ok: false,
      error: "disk full",
    });
    await expect(p).rejects.toThrow("disk full");
  });

  it("listLuaFiles rejects when native responds with ok: false", async () => {
    const { posted, deliver } = captureEngineBridgePosts();
    const p = listLuaFiles();
    const msg = posted.find((m) => m.op === "listLua");
    deliver({
      requestId: String(msg?.requestId ?? ""),
      ok: false,
      error: "Lua workspace not configured",
    });
    await expect(p).rejects.toThrow("Lua workspace not configured");
  });
});
