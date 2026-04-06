import type * as Monaco from "monaco-editor";
import {
  conf,
  language as luaLanguageDef,
} from "monaco-editor/esm/vs/basic-languages/lua/lua";

const LUAU_EXTRA_KEYWORDS = [
  "continue",
  "export",
  "type",
  "typeof",
  "readonly",
];

const ENGINE_COMPLETION_LABELS: string[] = [
  "draw.clear",
  "draw.line",
  "draw.circle",
  "draw.poly",
  "draw.char",
  "draw.number",
  "draw.present",
  "input.down",
  "audio.play",
  "audio.thrust",
  "app.quit",
  "screen.w",
  "screen.h",
  "require",
  "runtime.spawn",
  "runtime.destroy",
  "runtime.clear",
  "runtime.loadScene",
  "runtime.addScript",
  "runtime.setScriptProperty",
  "runtime.removeScript",
  "runtime.reorderScript",
  "runtime.setDrawOrder",
  "runtime.setUpdateOrder",
  "runtime.setName",
  "runtime.setActive",
  "runtime.setParent",
  "runtime.removeParent",
  "runtime.setTransform",
  "runtime.getTransform",
  "runtime.registerBehavior",
  "runtime.drawBehaviors",
  "defineProperties",
  "prop.number",
  "prop.integer",
  "prop.string",
  "prop.boolean",
  "prop.enum",
  "prop.object",
  "prop.asset",
  "prop.color",
  "prop.vector",
];

export function registerLuauEditorFeatures(monaco: typeof Monaco): void {
  monaco.languages.register({ id: "luau" });
  monaco.languages.setLanguageConfiguration("luau", conf);

  const language = {
    ...luaLanguageDef,
    tokenPostfix: ".luau",
    keywords: [...luaLanguageDef.keywords, ...LUAU_EXTRA_KEYWORDS],
  };
  monaco.languages.setMonarchTokensProvider("luau", language);

  monaco.languages.registerCompletionItemProvider("luau", {
    provideCompletionItems(model, position) {
      const word = model.getWordUntilPosition(position);
      const range = {
        startLineNumber: position.lineNumber,
        endLineNumber: position.lineNumber,
        startColumn: word.startColumn,
        endColumn: word.endColumn,
      };
      const prefix = word.word;
      const suggestions = ENGINE_COMPLETION_LABELS.filter(
        (label) => !prefix || label.startsWith(prefix) || label.includes(prefix),
      ).map((label) => ({
        label,
        kind: monaco.languages.CompletionItemKind.Method,
        insertText: label,
        range,
      }));
      return { suggestions };
    },
  });
}
