import Editor from "@monaco-editor/react";
import * as monaco from "monaco-editor";
import { useCallback, useEffect, useMemo, useRef } from "react";
import { useShallow } from "zustand/shallow";
import { registerLuauEditorFeatures } from "./monacoLuau";
import { readProjectFile, writeProjectFile } from "./projectFileBridge";
import { useLuaEditorStore } from "./luaEditorStore";
import "./LuaEditorPanel.css";

export type LuaEditorOpenRequest = {
  path: string;
  /** Optional buffer before `readProjectFile` (e.g. just-written file). */
  content?: string;
  /** Bump on each open so the same path can be opened again. */
  id: number;
};

type LuaEditorPanelProps = {
  openFileRequest?: LuaEditorOpenRequest | null;
  onConsumedOpenFileRequest?: () => void;
  /** Called after save or programmatic open so sibling views (e.g. Asset browser) can re-list. */
  onFilesMutated?: () => void;
};

function monacoLanguageForPath(path: string | null): string {
  if (!path) return "plaintext";
  const lower = path.toLowerCase();
  if (lower.endsWith(".lua") || path.startsWith("editor/")) return "luau";
  if (lower.endsWith(".json") || lower.endsWith(".scene")) return "json";
  return "plaintext";
}

export function LuaEditorPanel({
  openFileRequest,
  onConsumedOpenFileRequest,
  onFilesMutated,
}: LuaEditorPanelProps) {
  const {
    tabs,
    activePath,
    loadError,
    setActivePath,
    upsertTab,
    setBuffer,
    closeTab,
    setLoadError,
  } = useLuaEditorStore(
    useShallow((s) => ({
      tabs: s.tabs,
      activePath: s.activePath,
      loadError: s.loadError,
      setActivePath: s.setActivePath,
      upsertTab: s.upsertTab,
      setBuffer: s.setBuffer,
      closeTab: s.closeTab,
      setLoadError: s.setLoadError,
    })),
  );

  const activeTab = useMemo(
    () => tabs.find((t) => t.path === activePath) ?? null,
    [tabs, activePath],
  );

  const notifyFilesMutated = useCallback(() => {
    onFilesMutated?.();
  }, [onFilesMutated]);

  useEffect(() => {
    if (!openFileRequest) return;
    const { path, content } = openFileRequest;
    let cancelled = false;
    void (async () => {
      let initial: string;
      if (content !== undefined) {
        initial = content;
      } else {
        try {
          initial = await readProjectFile(path);
        } catch (e) {
          if (!cancelled) {
            setLoadError(e instanceof Error ? e.message : String(e));
          }
          return;
        }
      }
      if (cancelled) return;
      upsertTab(path, initial, false);
      notifyFilesMutated();
      onConsumedOpenFileRequest?.();
    })();
    return () => {
      cancelled = true;
    };
  }, [
    openFileRequest,
    upsertTab,
    notifyFilesMutated,
    onConsumedOpenFileRequest,
    setLoadError,
  ]);

  const onSave = useCallback(async () => {
    if (!activePath || !activeTab) return;
    try {
      await writeProjectFile(activePath, activeTab.content);
      setBuffer(activePath, activeTab.content, false);
      notifyFilesMutated();
    } catch (e) {
      setLoadError(e instanceof Error ? e.message : String(e));
    }
  }, [activePath, activeTab, setBuffer, setLoadError, notifyFilesMutated]);

  const onSaveRef = useRef(onSave);
  onSaveRef.current = onSave;

  const beforeMount = useCallback((m: typeof monaco) => {
    registerLuauEditorFeatures(m);
  }, []);

  const onMount = useCallback(
    (editor: monaco.editor.IStandaloneCodeEditor, m: typeof monaco) => {
      editor.addCommand(m.KeyMod.CtrlCmd | m.KeyCode.KeyS, () => {
        void onSaveRef.current();
      });
    },
    [],
  );

  const editorValue = activeTab?.content ?? "";
  const handleEditorChange = useCallback(
    (v: string | undefined) => {
      if (!activePath) return;
      const next = v ?? "";
      setBuffer(activePath, next, true);
    },
    [activePath, setBuffer],
  );

  return (
    <div className="lua-editor-panel">
      {loadError ? (
        <p className="lua-editor-error">{loadError}</p>
      ) : null}
      <div className="lua-editor-main">
        {tabs.length > 0 ? (
          <div className="lua-editor-tabs" role="tablist">
            {tabs.map((t) => (
              <div
                key={t.path}
                className={
                  t.path === activePath
                    ? "lua-editor-tab lua-editor-tab-active"
                    : "lua-editor-tab"
                }
                role="presentation"
              >
                <button
                  type="button"
                  className="lua-editor-tab-label"
                  role="tab"
                  aria-selected={t.path === activePath}
                  onClick={() => setActivePath(t.path)}
                >
                  {t.path}
                  {t.dirty ? " ·" : ""}
                </button>
                <button
                  type="button"
                  className="lua-editor-tab-close"
                  aria-label={`Close ${t.path}`}
                  onClick={() => closeTab(t.path)}
                >
                  ×
                </button>
              </div>
            ))}
          </div>
        ) : null}
        <div className="lua-editor-monaco">
          <Editor
            height="100%"
            width="100%"
            language={monacoLanguageForPath(activePath)}
            theme="vs-dark"
            path={activePath ?? "untitled"}
            value={editorValue}
            onChange={handleEditorChange}
            beforeMount={beforeMount}
            onMount={onMount}
            options={{
              minimap: { enabled: false },
              fontSize: 13,
              scrollBeyondLastLine: false,
              automaticLayout: true,
              readOnly: !activePath,
            }}
          />
        </div>
      </div>
    </div>
  );
}
