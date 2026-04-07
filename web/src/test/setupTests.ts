import "@testing-library/jest-dom/vitest";
import { createElement, useEffect, useRef } from "react";
import { afterEach, vi } from "vitest";
import { resetLuaEditorStoreForTests } from "../luaEditorStore";

afterEach(() => {
  resetLuaEditorStoreForTests();
});

globalThis.ResizeObserver = class ResizeObserver {
  observe(): void {}
  unobserve(): void {}
  disconnect(): void {}
};

Object.defineProperty(window, "matchMedia", {
  writable: true,
  value: vi.fn().mockImplementation((query: string) => ({
    matches: false,
    media: query,
    onchange: null,
    addListener: vi.fn(),
    removeListener: vi.fn(),
    addEventListener: vi.fn(),
    removeEventListener: vi.fn(),
    dispatchEvent: vi.fn(),
  })),
});

/** Matches monaco.KeyMod.CtrlCmd | monaco.KeyCode.KeyS */
const MOCK_SAVE_CHORD = 2048 | 49;

vi.mock("@monaco-editor/react", () => ({
  default: function MockMonacoEditor({
    value,
    onChange,
    onMount,
  }: {
    value?: string;
    onChange?: (v: string | undefined) => void;
    onMount?: (
      editor: { addCommand: (key: number, fn: () => void) => void },
      monacoLike: { KeyMod: { CtrlCmd: number }; KeyCode: { KeyS: number } },
    ) => void;
  }) {
    const saveRef = useRef<(() => void) | null>(null);
    useEffect(() => {
      if (!onMount) return;
      const editor = {
        addCommand: (key: number, fn: () => void) => {
          if (key === MOCK_SAVE_CHORD) saveRef.current = fn;
        },
      };
      const monacoStub = { KeyMod: { CtrlCmd: 2048 }, KeyCode: { KeyS: 49 } };
      onMount(editor, monacoStub);
    }, [onMount]);
    return createElement("textarea", {
      "data-testid": "monaco-stub",
      value: value ?? "",
      onChange: (e: Event) =>
        onChange?.((e.target as HTMLTextAreaElement).value),
      onKeyDown: (e: KeyboardEvent) => {
        if ((e.ctrlKey || e.metaKey) && e.key.toLowerCase() === "s") {
          e.preventDefault();
          saveRef.current?.();
        }
      },
    });
  },
}));
