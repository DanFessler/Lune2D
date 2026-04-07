import type { EngineScriptResponse } from "./luaEditorBridge";

export type ProjectDirEntry = {
  name: string;
  path: string;
  isDirectory: boolean;
  size: number;
  mtime?: string;
  thumbnail?: string;
};

type ListProjectDirResponse = EngineScriptResponse & { entries?: ProjectDirEntry[] };

type ReadProjectFileResponse = EngineScriptResponse & { content?: string };

export async function listProjectDir(dir = ""): Promise<ProjectDirEntry[]> {
  const bridge = window.__engineScriptBridge;
  if (!bridge) throw new Error("Engine script bridge not installed");
  const res = await bridge.call<ListProjectDirResponse>("listProjectDir", { dir });
  return Array.isArray(res.entries) ? res.entries : [];
}

export async function readProjectFile(
  path: string,
  opts?: { encoding?: "utf8" | "base64" },
): Promise<string> {
  const bridge = window.__engineScriptBridge;
  if (!bridge) throw new Error("Engine script bridge not installed");
  const body: Record<string, unknown> = { path };
  if (opts?.encoding) body.encoding = opts.encoding;
  const res = await bridge.call<ReadProjectFileResponse>("readProjectFile", body);
  return typeof res.content === "string" ? res.content : "";
}

export async function writeProjectFile(path: string, content: string): Promise<void> {
  await window.__engineScriptBridge!.call("writeProjectFile", { path, content });
}
