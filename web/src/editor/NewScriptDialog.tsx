import { useCallback, useEffect, useRef, useState } from "react";
import { engine } from "../engineProxy";
import { reloadBehaviors, writeLuaFile } from "../luaEditorBridge";
import {
  behaviorLifecycleTemplate,
  behaviorRelativePath,
  editorBehaviorLifecycleTemplate,
  editorBehaviorRelativePath,
  sanitizeBehaviorBaseName,
} from "./behaviorScriptTemplate";
import styles from "./NewScriptDialog.module.css";

export function NewScriptDialog({
  open,
  onClose,
  entityId,
  onOpenLuaFile,
  onScriptsChanged,
}: {
  open: boolean;
  onClose: () => void;
  entityId: number;
  onOpenLuaFile: (path: string, content?: string) => void;
  onScriptsChanged?: () => void;
}) {
  const [draftName, setDraftName] = useState("NewBehavior");
  const [nameError, setNameError] = useState<string | null>(null);
  const [createEditorPair, setCreateEditorPair] = useState(false);
  const inputRef = useRef<HTMLInputElement>(null);

  useEffect(() => {
    if (!open) return;
    setDraftName("NewBehavior");
    setNameError(null);
    setCreateEditorPair(false);
    queueMicrotask(() => {
      inputRef.current?.focus();
      inputRef.current?.select();
    });
  }, [open]);

  const createNewScript = useCallback(async () => {
    const base = sanitizeBehaviorBaseName(draftName);
    if (!base) {
      setNameError(
        "Invalid name. Use letters, numbers, underscores; start with a letter or _.",
      );
      return;
    }
    setNameError(null);
    const relPath = behaviorRelativePath(base);
    const template = behaviorLifecycleTemplate(base);
    try {
      await writeLuaFile(relPath, template);
      if (createEditorPair) {
        const edPath = editorBehaviorRelativePath(base);
        const edTemplate = editorBehaviorLifecycleTemplate(base);
        await writeLuaFile(edPath, edTemplate);
      }
      await reloadBehaviors();
      await engine.runtime.addScript(entityId, base);
      onOpenLuaFile(relPath, template);
      onScriptsChanged?.();
      onClose();
    } catch (e) {
      setNameError(e instanceof Error ? e.message : String(e));
    }
  }, [draftName, entityId, onClose, onOpenLuaFile, onScriptsChanged]);

  if (!open) return null;

  return (
    <div
      className={styles.backdrop}
      onMouseDown={(e) => {
        if (e.target === e.currentTarget) onClose();
      }}
    >
      <div
        className={styles.dialog}
        role="dialog"
        aria-labelledby="new-script-title"
        aria-modal="true"
      >
        <h2 id="new-script-title" className={styles.title}>
          New behavior script
        </h2>
        <div className={styles.field}>
          <label htmlFor="new-script-name-input" className={styles.label}>
            Behavior name (e.g. CoinPickup)
          </label>
          <input
            id="new-script-name-input"
            ref={inputRef}
            className={styles.input}
            value={draftName}
            aria-label="New behavior name"
            onChange={(e) => {
              setDraftName(e.target.value);
              setNameError(null);
            }}
            onKeyDown={(e) => {
              if (e.key === "Enter") {
                e.preventDefault();
                void createNewScript();
              }
              if (e.key === "Escape") onClose();
            }}
          />
          {nameError ? (
            <p className={styles.formError} role="alert">
              {nameError}
            </p>
          ) : null}
        </div>
        <div className={styles.field}>
          <label className={styles.label} style={{ display: "flex", alignItems: "center", gap: 6 }}>
            <input
              type="checkbox"
              checked={createEditorPair}
              onChange={(e) => setCreateEditorPair(e.target.checked)}
            />
            Also create editor behavior
          </label>
        </div>
        <div className={styles.actions}>
          <button type="button" className={styles.btn} onClick={onClose}>
            Cancel
          </button>
          <button
            type="button"
            className={`${styles.btn} ${styles.btnPrimary}`}
            onClick={() => void createNewScript()}
          >
            Create
          </button>
        </div>
      </div>
    </div>
  );
}
