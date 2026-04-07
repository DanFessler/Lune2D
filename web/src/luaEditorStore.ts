import { create } from "zustand";

export type LuaEditorTab = {
  path: string;
  content: string;
  dirty: boolean;
};

export type LuaEditorSlice = {
  tabs: LuaEditorTab[];
  activePath: string | null;
  loadError: string | null;
  status: string | null;
  setActivePath: (path: string | null) => void;
  upsertTab: (path: string, content: string, dirty: boolean) => void;
  /** Updates buffer for `path`; no-op if tab missing. */
  setBuffer: (path: string, content: string, dirty: boolean) => void;
  closeTab: (path: string) => void;
  setLoadError: (msg: string | null) => void;
  setStatus: (msg: string | null) => void;
};

const initialState = {
  tabs: [] as LuaEditorTab[],
  activePath: null as string | null,
  loadError: null as string | null,
  status: null as string | null,
};

export const useLuaEditorStore = create<LuaEditorSlice>((set) => ({
  ...initialState,

  setActivePath: (path) => set({ activePath: path }),

  upsertTab: (path, content, dirty) =>
    set((s) => {
      const i = s.tabs.findIndex((t) => t.path === path);
      if (i >= 0) {
        const next = [...s.tabs];
        next[i] = { path, content, dirty };
        return { tabs: next, activePath: path, loadError: null };
      }
      return {
        tabs: [...s.tabs, { path, content, dirty }],
        activePath: path,
        loadError: null,
      };
    }),

  setBuffer: (path, content, dirty) =>
    set((s) => {
      const i = s.tabs.findIndex((t) => t.path === path);
      if (i < 0) return s;
      const next = [...s.tabs];
      next[i] = { path, content, dirty };
      return { tabs: next };
    }),

  closeTab: (path) =>
    set((s) => {
      const idx = s.tabs.findIndex((t) => t.path === path);
      if (idx < 0) return s;
      const next = s.tabs.filter((t) => t.path !== path);
      let activePath = s.activePath;
      if (activePath === path) {
        const neighbor = next[Math.min(idx, Math.max(0, next.length - 1))];
        activePath = neighbor ? neighbor.path : null;
      }
      return { tabs: next, activePath };
    }),

  setLoadError: (msg) => set({ loadError: msg }),

  setStatus: (msg) => set({ status: msg }),
}));

/** Vitest: isolate editor state between tests. */
export function resetLuaEditorStoreForTests() {
  useLuaEditorStore.setState({ ...initialState });
}
