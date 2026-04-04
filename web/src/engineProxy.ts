import type { EngineApi, SceneOpResult } from "./generated/sceneOps";

/**
 * Send a scene-mutation op to native via the existing engineScript bridge.
 * Returns the reply (or throws on error / bridge unavailable).
 */
export async function invokeSceneOp(
  op: string,
  args: unknown[],
): Promise<SceneOpResult> {
  const bridge = window.__engineScriptBridge;
  if (!bridge) throw new Error("Engine bridge not installed");
  const res = await bridge.call<SceneOpResult & { requestId: string; ok: boolean }>(op, { args });
  if (!res.ok) throw new Error(res.error ?? `Scene op "${op}" failed`);
  return res;
}

/** For ops like `runtime.spawn` whose native handler sets `result` to an entity id. */
export function unwrapSceneNumber(res: SceneOpResult): number {
  const r = res.result;
  if (typeof r === "number" && Number.isFinite(r)) return r;
  if (typeof r === "string" && r !== "") {
    const n = Number(r);
    if (Number.isFinite(n)) return n;
  }
  throw new Error("Expected numeric scene op result");
}

const TRAP_BLACKLIST = new Set([
  "then",
  "toJSON",
  "valueOf",
  "toString",
  "$$typeof",
  "asymmetricMatch",
  Symbol.toPrimitive as unknown as string,
  Symbol.toStringTag as unknown as string,
]);

function createNamespace(path: string[]): unknown {
  const fn = function () {};
  return new Proxy(fn, {
    get(_, prop) {
      if (typeof prop === "symbol" || TRAP_BLACKLIST.has(prop as string))
        return undefined;
      return createNamespace([...path, prop as string]);
    },
    apply(_, __, args: unknown[]) {
      const op = path.join(".");
      return invokeSceneOp(op, args);
    },
  });
}

/**
 * Proxy-based engine API. Property accesses build the op string; calling
 * invokes it over the bridge.
 *
 * Usage: `await engine.runtime.setName(7, "Ship")`
 */
export const engine = createNamespace([]) as EngineApi;
