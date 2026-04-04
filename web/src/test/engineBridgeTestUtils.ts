import type { EngineScriptResponse } from "../luaEditorBridge";
import {
  installEngineScriptBridgeShim,
  resetEngineScriptBridgeForTests,
} from "../luaEditorBridge";

/** Capture native posts and manually deliver responses (Vitest). */
export function captureEngineBridgePosts() {
  const posted: Record<string, unknown>[] = [];
  resetEngineScriptBridgeForTests();
  installEngineScriptBridgeShim((msg) => posted.push(msg));
  return {
    posted,
    deliver(res: EngineScriptResponse) {
      window.__engineScriptBridge!.receive(res);
    },
    lastRequestId(): string {
      const last = posted[posted.length - 1];
      return String(last?.requestId ?? "");
    },
  };
}
