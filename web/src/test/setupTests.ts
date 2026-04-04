import "@testing-library/jest-dom/vitest";
import { createElement } from "react";
import { vi } from "vitest";

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

vi.mock("@monaco-editor/react", () => ({
  default: function MockMonacoEditor({
    value,
    onChange,
  }: {
    value?: string;
    onChange?: (v: string | undefined) => void;
  }) {
    return createElement("textarea", {
      "data-testid": "monaco-stub",
      value: value ?? "",
      onChange: (e: Event) =>
        onChange?.((e.target as HTMLTextAreaElement).value),
    });
  },
}));
