import { afterEach, describe, expect, it } from "vitest";
import { captureEngineBridgePosts } from "./test/engineBridgeTestUtils";
import { installEngineScriptBridgeShim, resetEngineScriptBridgeForTests } from "./luaEditorBridge";
import { listProjectDir, readProjectFile, writeProjectFile } from "./projectFileBridge";

afterEach(() => {
  resetEngineScriptBridgeForTests();
});

describe("projectFileBridge", () => {
  it("listProjectDir posts listProjectDir with dir empty string", async () => {
    const { posted, deliver } = captureEngineBridgePosts();
    const p = listProjectDir("");
    expect(posted.some((m) => m.op === "listProjectDir")).toBe(true);
    const msg = posted.find((m) => m.op === "listProjectDir");
    expect(msg?.dir).toBe("");
    deliver({
      requestId: String(msg?.requestId ?? ""),
      ok: true,
      entries: [
        {
          name: "behaviors",
          path: "behaviors",
          isDirectory: true,
          size: 0,
        },
      ],
    });
    const entries = await p;
    expect(entries).toHaveLength(1);
    expect(entries[0].path).toBe("behaviors");
    expect(entries[0].isDirectory).toBe(true);
  });

  it("listProjectDir(behaviors) passes dir", async () => {
    const { posted, deliver } = captureEngineBridgePosts();
    const p = listProjectDir("behaviors");
    const msg = posted.find((m) => m.op === "listProjectDir");
    expect(msg?.dir).toBe("behaviors");
    deliver({
      requestId: String(msg?.requestId ?? ""),
      ok: true,
      entries: [
        {
          name: "Ship.lua",
          path: "behaviors/Ship.lua",
          isDirectory: false,
          size: 12,
          mtime: "2020-01-01T00:00:00.000Z",
        },
      ],
    });
    const entries = await p;
    expect(entries[0].path).toBe("behaviors/Ship.lua");
  });
});

describe("readProjectFile", () => {
  it("posts readProjectFile with path", async () => {
    const { posted, deliver } = captureEngineBridgePosts();
    const p = readProjectFile("behaviors/Ship.lua");
    const msg = posted.find((m) => m.op === "readProjectFile");
    expect(msg?.path).toBe("behaviors/Ship.lua");
    deliver({
      requestId: String(msg?.requestId ?? ""),
      ok: true,
      content: "-- x",
    });
    await expect(p).resolves.toBe("-- x");
  });

  it("rejects on ok false", async () => {
    const { posted, deliver } = captureEngineBridgePosts();
    const p = readProjectFile("missing.lua");
    const msg = posted.find((m) => m.op === "readProjectFile");
    deliver({
      requestId: String(msg?.requestId ?? ""),
      ok: false,
      error: "not found",
    });
    await expect(p).rejects.toThrow("not found");
  });
});

describe("writeProjectFile", () => {
  it("posts path and content", async () => {
    const { posted, deliver } = captureEngineBridgePosts();
    const p = writeProjectFile("game/x.lua", "y=1");
    const msg = posted.find((m) => m.op === "writeProjectFile");
    expect(msg?.path).toBe("game/x.lua");
    expect(msg?.content).toBe("y=1");
    deliver({ requestId: String(msg?.requestId ?? ""), ok: true });
    await p;
  });
});
