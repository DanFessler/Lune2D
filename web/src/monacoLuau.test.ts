import { describe, expect, it, vi } from "vitest";
import { registerLuauEditorFeatures } from "./monacoLuau";

type FakeMonaco = {
  languages: {
    register: ReturnType<typeof vi.fn>;
    setLanguageConfiguration: ReturnType<typeof vi.fn>;
    setMonarchTokensProvider: ReturnType<typeof vi.fn>;
    registerCompletionItemProvider: ReturnType<typeof vi.fn>;
    CompletionItemKind: { Method: number };
  };
};

function fakeMonaco(): FakeMonaco {
  return {
    languages: {
      register: vi.fn(),
      setLanguageConfiguration: vi.fn(),
      setMonarchTokensProvider: vi.fn(),
      registerCompletionItemProvider: vi.fn(),
      CompletionItemKind: { Method: 1 },
    },
  };
}

describe("registerLuauEditorFeatures", () => {
  it("registers a luau language id for syntax highlighting", () => {
    const monaco = fakeMonaco();
    registerLuauEditorFeatures(monaco as never);
    expect(monaco.languages.register).toHaveBeenCalledWith(
      expect.objectContaining({ id: "luau" }),
    );
    expect(monaco.languages.setLanguageConfiguration).toHaveBeenCalledWith(
      "luau",
      expect.any(Object),
    );
    expect(monaco.languages.setMonarchTokensProvider).toHaveBeenCalledWith(
      "luau",
      expect.any(Object),
    );
  });

  it("registers a completion provider with engine API members", async () => {
    const monaco = fakeMonaco();
    registerLuauEditorFeatures(monaco as never);
    const call =
      monaco.languages.registerCompletionItemProvider.mock.calls.find(
        (c) => c[0] === "luau",
      );
    expect(call).toBeTruthy();
    const provider = call![1] as {
      provideCompletionItems: (
        model: { getWordUntilPosition: () => { word: string } },
        position: unknown,
      ) =>
        | Promise<{ suggestions: { label: string }[] }>
        | { suggestions: { label: string }[] };
    };
    const model = {
      getWordUntilPosition: () => ({ word: "draw" }),
    };
    const out = await provider.provideCompletionItems(model, {} as never);
    const labels = out.suggestions.map((s) => s.label);
    expect(labels.some((l) => l.includes("draw."))).toBe(true);
  });
});
