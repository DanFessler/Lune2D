export type LuaWorkspaceFile = {
  path: string;
  content: string;
};

export type EngineScriptResponse = {
  requestId: string;
  ok: boolean;
  error?: string;
  files?: LuaWorkspaceFile[];
  /** Scene ops (e.g. runtime.spawn) return the native result here */
  result?: unknown;
};

type Pending = (r: EngineScriptResponse) => void;

export type EngineScriptBridgeHost = {
  send: (msg: Record<string, unknown>) => void;
  receive: (res: EngineScriptResponse) => void;
  call: <T extends EngineScriptResponse>(
    op: string,
    body?: Record<string, unknown>,
  ) => Promise<T>;
};

declare global {
  interface Window {
    __engineScriptBridge?: EngineScriptBridgeHost;
    webkit?: {
      messageHandlers?: {
        engineScript?: { postMessage: (body: unknown) => void };
      };
    };
  }
}

let shimInstalled = false;

export function resetEngineScriptBridgeForTests() {
  shimInstalled = false;
  delete window.__engineScriptBridge;
}

function defaultPostMessage(msg: Record<string, unknown>) {
  const h = window.webkit?.messageHandlers?.engineScript;
  if (h) {
    h.postMessage(msg);
    return;
  }
  const rid = typeof msg.requestId === "string" ? msg.requestId : "";
  if (rid && window.__engineScriptBridge) {
    queueMicrotask(() =>
      window.__engineScriptBridge!.receive({
        requestId: rid,
        ok: false,
        error: "Native engine bridge unavailable (run the desktop app).",
      }),
    );
  }
}

/**
 * Installs window.__engineScriptBridge for Promise-based native IPC.
 * In tests, call `resetEngineScriptBridgeForTests()` first, then pass a capturing `postMessageImpl`.
 */
export function installEngineScriptBridgeShim(
  postMessageImpl: (msg: Record<string, unknown>) => void = defaultPostMessage,
) {
  if (shimInstalled) return;

  const pending = new Map<string, Pending>();

  const bridge: EngineScriptBridgeHost = {
    send(msg) {
      postMessageImpl(msg);
    },
    receive(res) {
      const cb = pending.get(res.requestId);
      if (!cb) return;
      pending.delete(res.requestId);
      cb(res);
    },
    call<T extends EngineScriptResponse>(
      op: string,
      body: Record<string, unknown> = {},
    ): Promise<T> {
      return new Promise((resolve, reject) => {
        const requestId = `es-${Date.now()}-${Math.random().toString(36).slice(2)}`;
        pending.set(requestId, (r) => {
          if (r.ok) resolve(r as T);
          else reject(new Error(r.error ?? "Engine script request failed"));
        });
        bridge.send({ requestId, op, ...body });
      });
    },
  };

  window.__engineScriptBridge = bridge;
  shimInstalled = true;
}

export function isEngineScriptBridgeAvailable(): boolean {
  return !!window.webkit?.messageHandlers?.engineScript;
}

type ListLuaResponse = EngineScriptResponse & { files?: LuaWorkspaceFile[] };

export async function listLuaFiles(): Promise<LuaWorkspaceFile[]> {
  if (!window.__engineScriptBridge) {
    throw new Error("Engine script bridge not installed");
  }
  const res = await window.__engineScriptBridge.call<ListLuaResponse>("listLua", {});
  return Array.isArray(res.files) ? res.files : [];
}

export async function writeLuaFile(path: string, content: string): Promise<void> {
  await window.__engineScriptBridge!.call("writeLua", { path, content });
}

/**
 * Full Luau VM reset (Stop). Restores the scene hierarchy snapshot taken when Play last started
 * from a stopped state; otherwise reloads the default scene from disk.
 */
export async function reloadScripts(): Promise<void> {
  await window.__engineScriptBridge!.call("restartGame", {});
}

/** Rescan `behaviors/*.lua` into _BEHAVIORS (same VM, scene preserved). Runs synchronously on native before ACK. */
export async function reloadBehaviors(): Promise<void> {
  await window.__engineScriptBridge!.call("reloadBehaviors", {});
}

/**
 * Begin or resume simulation (HUD play / unpause).
 * When `captureEditorSnapshot` is true (entering play from stopped), native saves the current
 * scene for restore on Stop. When false (resuming from pause), the prior snapshot is unchanged.
 */
export async function startSimulation(captureEditorSnapshot = true): Promise<void> {
  await window.__engineScriptBridge!.call("startSimulation", {
    captureEditorSnapshot,
  });
}

export async function setGamePaused(paused: boolean): Promise<void> {
  await window.__engineScriptBridge!.call("setPaused", { paused });
}

type ListBehaviorsResponse = EngineScriptResponse & { behaviors?: string[] };

export async function listBehaviors(): Promise<string[]> {
  if (!window.__engineScriptBridge) return [];
  const res = await window.__engineScriptBridge.call<ListBehaviorsResponse>("listBehaviors", {});
  return Array.isArray(res.behaviors) ? res.behaviors : [];
}
