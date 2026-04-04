export type LuaWorkspaceFile = {
  path: string;
  content: string;
};

export type EngineScriptResponse = {
  requestId: string;
  ok: boolean;
  error?: string;
  files?: LuaWorkspaceFile[];
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

/** Reload all Luau scripts from disk (full VM reset). Lands on title screen until Start. */
export async function reloadScripts(): Promise<void> {
  await window.__engineScriptBridge!.call("restartGame", {});
}

/** Begin or resume simulation (title → first wave, or clears engine pause). */
export async function startSimulation(): Promise<void> {
  await window.__engineScriptBridge!.call("startSimulation", {});
}

export async function setGamePaused(paused: boolean): Promise<void> {
  await window.__engineScriptBridge!.call("setPaused", { paused });
}
