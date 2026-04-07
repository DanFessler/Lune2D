import { describe, it, expect, beforeEach, afterEach } from "vitest";
import {
  resetEngineScriptBridgeForTests,
  installEngineScriptBridgeShim,
} from "./luaEditorBridge";
import { engine } from "./engineProxy";

let posted: Record<string, unknown>[];

function deliver(res: { requestId: string; ok: boolean; result?: unknown; error?: string }) {
  window.__engineScriptBridge!.receive({
    ...res,
    requestId: res.requestId,
    ok: res.ok,
  });
}

beforeEach(() => {
  posted = [];
  resetEngineScriptBridgeForTests();
  installEngineScriptBridgeShim((msg) => posted.push(msg));
});

afterEach(() => {
  resetEngineScriptBridgeForTests();
});

describe("engine proxy", () => {
  it("sends runtime.setName with correct op and args", async () => {
    const p = engine.runtime.setName(7, "Ship");
    expect(posted.length).toBe(1);
    expect(posted[0].op).toBe("runtime.setName");
    expect(posted[0].args).toEqual([7, "Ship"]);

    deliver({ requestId: String(posted[0].requestId), ok: true });
    await p;
  });

  it("sends runtime.loadScene with relative path", async () => {
    const p = engine.runtime.loadScene("scenes/default.scene.json");
    expect(posted[0].op).toBe("runtime.loadScene");
    expect(posted[0].args).toEqual(["scenes/default.scene.json"]);
    deliver({ requestId: String(posted[0].requestId), ok: true });
    await p;
  });

  it("sends runtime.spawn and returns the result", async () => {
    const p = engine.runtime.spawn("Entity");
    expect(posted[0].op).toBe("runtime.spawn");
    expect(posted[0].args).toEqual(["Entity"]);

    deliver({ requestId: String(posted[0].requestId), ok: true, result: 42 });
    await expect(p).resolves.toEqual(
      expect.objectContaining({ ok: true, result: 42 }),
    );
  });

  it("sends runtime.setTransform with three args", async () => {
    const p = engine.runtime.setTransform(3, "x", 100.5);
    expect(posted[0].op).toBe("runtime.setTransform");
    expect(posted[0].args).toEqual([3, "x", 100.5]);

    deliver({ requestId: String(posted[0].requestId), ok: true });
    await p;
  });

  it("sends runtime.reorderScript with three args", async () => {
    const p = engine.runtime.reorderScript(1, 0, 2);
    expect(posted[0].op).toBe("runtime.reorderScript");
    expect(posted[0].args).toEqual([1, 0, 2]);

    deliver({ requestId: String(posted[0].requestId), ok: true });
    await p;
  });

  it("rejects when native returns ok: false", async () => {
    const p = engine.runtime.destroy(999);
    deliver({
      requestId: String(posted[0].requestId),
      ok: false,
      error: "Entity not found",
    });
    await expect(p).rejects.toThrow("Entity not found");
  });

  it("does not look like a Promise (no .then trap)", () => {
    expect((engine as unknown as Record<string, unknown>).then).toBeUndefined();
    expect(
      (engine.runtime as unknown as Record<string, unknown>).then,
    ).toBeUndefined();
  });
});
