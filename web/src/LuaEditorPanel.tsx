import Editor from "@monaco-editor/react";
import * as monaco from "monaco-editor";
import { useCallback, useEffect, useState } from "react";
import {
  listLuaFiles,
  writeLuaFile,
  type LuaWorkspaceFile,
} from "./luaEditorBridge";
import { registerLuauEditorFeatures } from "./monacoLuau";
import "./LuaEditorPanel.css";

export type LuaEditorOpenRequest = {
  path: string;
  /** Optional buffer to show before listLua returns (e.g. just-written file). */
  content?: string;
  /** Bump on each open so the same path can be opened again. */
  id: number;
};

type LuaEditorPanelProps = {
  openFileRequest?: LuaEditorOpenRequest | null;
  onConsumedOpenFileRequest?: () => void;
};

export function LuaEditorPanel({
  openFileRequest,
  onConsumedOpenFileRequest,
}: LuaEditorPanelProps) {
  const [files, setFiles] = useState<LuaWorkspaceFile[]>([]);
  const [activePath, setActivePath] = useState<string | null>(null);
  const [value, setValue] = useState("");
  const [status, setStatus] = useState<string | null>(null);
  const [loadError, setLoadError] = useState<string | null>(null);

  const refresh = useCallback(async () => {
    setLoadError(null);
    try {
      const list = await listLuaFiles();
      setFiles(list);
      if (list.length === 0) {
        setActivePath(null);
        setValue("");
        setStatus("No .lua files in workspace.");
        return;
      }
      const preferred = list[0].path;
      setActivePath((prev) =>
        list.some((f) => f.path === prev) ? prev! : preferred,
      );
      setStatus(null);
    } catch (e) {
      setLoadError(e instanceof Error ? e.message : String(e));
    }
  }, []);

  useEffect(() => {
    void refresh();
  }, [refresh]);

  useEffect(() => {
    if (!activePath) return;
    const f = files.find((x) => x.path === activePath);
    if (f) setValue(f.content);
  }, [activePath, files]);

  useEffect(() => {
    if (!openFileRequest) return;
    const { path, content } = openFileRequest;
    let cancelled = false;
    void (async () => {
      if (content !== undefined) setValue(content);
      await refresh();
      if (cancelled) return;
      setActivePath(path);
      onConsumedOpenFileRequest?.();
    })();
    return () => {
      cancelled = true;
    };
  }, [openFileRequest?.id, openFileRequest?.path, openFileRequest?.content, refresh, onConsumedOpenFileRequest]);

  const onSave = useCallback(async () => {
    if (!activePath) return;
    setStatus(null);
    try {
      await writeLuaFile(activePath, value);
      setStatus("Saved.");
      await refresh();
    } catch (e) {
      setStatus(e instanceof Error ? e.message : String(e));
    }
  }, [activePath, refresh, value]);

  const beforeMount = useCallback((m: typeof monaco) => {
    registerLuauEditorFeatures(m);
  }, []);

  return (
    <div className="lua-editor-panel">
      <div className="lua-editor-toolbar">
        <label className="lua-editor-label">
          File
          <select
            value={activePath ?? ""}
            onChange={(e) => setActivePath(e.target.value || null)}
            disabled={files.length === 0}
          >
            {files.length === 0 ? (
              <option value="">—</option>
            ) : (
              files.map((f) => (
                <option key={f.path} value={f.path}>
                  {f.path}
                </option>
              ))
            )}
          </select>
        </label>
        <button type="button" onClick={() => void onSave()}>
          Save
        </button>
        <button type="button" onClick={() => void refresh()}>
          Refresh list
        </button>
      </div>
      {loadError ? (
        <p className="lua-editor-error">{loadError}</p>
      ) : null}
      {status ? <p className="lua-editor-status">{status}</p> : null}
      <div className="lua-editor-monaco">
        <Editor
          height="100%"
          width="100%"
          language="luau"
          theme="vs-dark"
          path={activePath ?? "untitled"}
          value={value}
          onChange={(v) => setValue(v ?? "")}
          beforeMount={beforeMount}
          options={{
            minimap: { enabled: false },
            fontSize: 13,
            scrollBeyondLastLine: false,
            automaticLayout: true,
          }}
        />
      </div>
    </div>
  );
}
