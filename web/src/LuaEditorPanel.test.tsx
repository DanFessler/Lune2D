import { act, fireEvent, render, screen, waitFor } from "@testing-library/react";
import userEvent from "@testing-library/user-event";
import { useCallback, useRef, useState } from "react";
import { afterEach, describe, expect, it, vi } from "vitest";
import AssetBrowser from "./editor/views/AssetBrowser";
import { engine } from "./engineProxy";
import {
  LuaEditorPanel,
  type LuaEditorOpenRequest,
} from "./LuaEditorPanel";
import { captureEngineBridgePosts } from "./test/engineBridgeTestUtils";
import { resetEngineScriptBridgeForTests } from "./luaEditorBridge";

afterEach(() => {
  resetEngineScriptBridgeForTests();
  vi.clearAllMocks();
});

function deliverListDir(
  posted: Record<string, unknown>[],
  deliver: (r: import("./luaEditorBridge").EngineScriptResponse) => void,
  dir: string,
  entries: Array<{
    name: string;
    path: string;
    isDirectory: boolean;
    size: number;
  }>,
) {
  const msg = posted.find(
    (m) => m.op === "listProjectDir" && String(m.dir ?? "") === dir,
  );
  if (!msg) return;
  deliver({
    requestId: String(msg.requestId ?? ""),
    ok: true,
    entries,
  });
}

/** ctx-game AssetBrowser labels files by stem (`name.split(".")[0]`). */
function doubleClickAssetStem(stem: string) {
  const label = screen.getByText(stem);
  const tile = label.parentElement;
  expect(tile).toBeTruthy();
  fireEvent.doubleClick(tile!);
}

function EditorWithAssetBrowser() {
  const [luaReq, setLuaReq] = useState<LuaEditorOpenRequest | null>(null);
  const [explorerKey, setExplorerKey] = useState(0);
  const seq = useRef(0);
  const focusLua = useCallback((path: string, content?: string) => {
    seq.current += 1;
    setLuaReq({ path, content, id: seq.current });
  }, []);
  return (
    <>
      <AssetBrowser
        refreshKey={explorerKey}
        onOpenEntry={(p) => focusLua(p)}
        onLoadScene={(p) => void engine.runtime.loadScene(p)}
      />
      <LuaEditorPanel
        openFileRequest={luaReq}
        onConsumedOpenFileRequest={() => setLuaReq(null)}
        onFilesMutated={() => setExplorerKey((k) => k + 1)}
      />
    </>
  );
}

describe("LuaEditorPanel", () => {
  it("does not render a file <select>", () => {
    render(<LuaEditorPanel />);
    expect(document.querySelector("select")).toBeNull();
  });

  it("opening a file via Asset browser double-click uses readProjectFile and shows a closeable tab", async () => {
    const { posted, deliver } = captureEngineBridgePosts();
    render(<EditorWithAssetBrowser />);
    await waitFor(() =>
      expect(posted.some((m) => m.op === "listProjectDir")).toBe(true),
    );
    deliverListDir(posted, deliver, "", [
      {
        name: "Ship.lua",
        path: "behaviors/Ship.lua",
        isDirectory: false,
        size: 12,
      },
    ]);
    await screen.findByText("Ship");
    doubleClickAssetStem("Ship");
    await waitFor(() =>
      expect(
        posted.some(
          (m) =>
            m.op === "readProjectFile" && m.path === "behaviors/Ship.lua",
        ),
      ).toBe(true),
    );
    const read = posted.find(
      (m) => m.op === "readProjectFile" && m.path === "behaviors/Ship.lua",
    );
    await act(async () => {
      deliver({
        requestId: String(read?.requestId ?? ""),
        ok: true,
        content: "-- initial",
      });
    });
    await waitFor(() =>
      expect(screen.getByTestId("monaco-stub")).toHaveValue("-- initial"),
    );
    expect(
      screen.getByRole("button", { name: /Close behaviors\/Ship\.lua/i }),
    ).toBeInTheDocument();
  });

  it("Ctrl/Cmd+S writes the active buffer via writeProjectFile", async () => {
    const user = userEvent.setup();
    const { posted, deliver } = captureEngineBridgePosts();
    render(<EditorWithAssetBrowser />);
    await waitFor(() =>
      expect(posted.some((m) => m.op === "listProjectDir")).toBe(true),
    );
    deliverListDir(posted, deliver, "", [
      {
        name: "Ship.lua",
        path: "behaviors/Ship.lua",
        isDirectory: false,
        size: 12,
      },
    ]);
    await screen.findByText("Ship");
    doubleClickAssetStem("Ship");
    await waitFor(() =>
      expect(posted.some((m) => m.op === "readProjectFile")).toBe(true),
    );
    const r0 = posted.filter((m) => m.op === "readProjectFile").pop();
    await act(async () => {
      deliver({
        requestId: String(r0?.requestId ?? ""),
        ok: true,
        content: "v1",
      });
    });
    await waitFor(() =>
      expect(screen.getByTestId("monaco-stub")).toHaveValue("v1"),
    );
    fireEvent.change(screen.getByTestId("monaco-stub"), {
      target: { value: "updated" },
    });
    const monaco = screen.getByTestId("monaco-stub");
    monaco.focus();
    await user.keyboard("{Control>}s{/Control}");
    await waitFor(() => {
      const w = posted.find((m) => m.op === "writeProjectFile");
      expect(w?.content).toBe("updated");
      expect(w?.path).toBe("behaviors/Ship.lua");
    });
  });

  it("double-click scene asset calls runtime.loadScene, not readProjectFile", async () => {
    const { posted, deliver } = captureEngineBridgePosts();
    render(<EditorWithAssetBrowser />);
    await waitFor(() =>
      expect(posted.some((m) => m.op === "listProjectDir")).toBe(true),
    );
    deliverListDir(posted, deliver, "", [
      {
        name: "level.scene.json",
        path: "scenes/level.scene.json",
        isDirectory: false,
        size: 4,
      },
    ]);
    await screen.findByText("level");
    doubleClickAssetStem("level");
    await waitFor(() =>
      expect(posted.some((m) => m.op === "runtime.loadScene")).toBe(true),
    );
    const ls = posted.find((m) => m.op === "runtime.loadScene");
    expect(ls?.args).toEqual(["scenes/level.scene.json"]);
    expect(
      posted.some((m) => m.op === "readProjectFile"),
    ).toBe(false);
    await act(async () => {
      deliver({
        requestId: String(ls?.requestId ?? ""),
        ok: true,
      });
    });
  });

  it("double-click plain scenes/foo.json uses readProjectFile, not loadScene", async () => {
    const { posted, deliver } = captureEngineBridgePosts();
    render(<EditorWithAssetBrowser />);
    await waitFor(() =>
      expect(posted.some((m) => m.op === "listProjectDir")).toBe(true),
    );
    deliverListDir(posted, deliver, "", [
      {
        name: "foo.json",
        path: "scenes/foo.json",
        isDirectory: false,
        size: 4,
      },
    ]);
    await screen.findByText("foo");
    doubleClickAssetStem("foo");
    await waitFor(() =>
      expect(posted.some((m) => m.op === "readProjectFile")).toBe(true),
    );
    expect(posted.some((m) => m.op === "runtime.loadScene")).toBe(false);
    const read = posted.find((m) => m.op === "readProjectFile");
    await act(async () => {
      deliver({
        requestId: String(read?.requestId ?? ""),
        ok: true,
        content: "{}",
      });
    });
    await waitFor(() =>
      expect(screen.getByTestId("monaco-stub")).toHaveValue("{}"),
    );
  });

  it("second open file adds a second tab", async () => {
    const { posted, deliver } = captureEngineBridgePosts();
    render(<EditorWithAssetBrowser />);
    await waitFor(() =>
      expect(posted.some((m) => m.op === "listProjectDir")).toBe(true),
    );
    deliverListDir(posted, deliver, "", [
      {
        name: "A.lua",
        path: "behaviors/A.lua",
        isDirectory: false,
        size: 1,
      },
      {
        name: "B.lua",
        path: "behaviors/B.lua",
        isDirectory: false,
        size: 1,
      },
    ]);
    await screen.findByText("A");
    doubleClickAssetStem("A");
    await waitFor(() =>
      expect(posted.some((m) => m.op === "readProjectFile")).toBe(true),
    );
    const r0 = posted.filter((m) => m.op === "readProjectFile").pop();
    await act(async () => {
      deliver({
        requestId: String(r0?.requestId ?? ""),
        ok: true,
        content: "a",
      });
    });
    doubleClickAssetStem("B");
    await waitFor(() =>
      expect(posted.filter((m) => m.op === "readProjectFile")).toHaveLength(
        2,
      ),
    );
    const r1 = posted.filter((m) => m.op === "readProjectFile").pop();
    await act(async () => {
      deliver({
        requestId: String(r1?.requestId ?? ""),
        ok: true,
        content: "b",
      });
    });
    expect(screen.getAllByRole("tab")).toHaveLength(2);
  });

  it("tab switch preserves unsaved buffer", async () => {
    const user = userEvent.setup();
    const { posted, deliver } = captureEngineBridgePosts();
    render(<EditorWithAssetBrowser />);
    await waitFor(() =>
      expect(posted.some((m) => m.op === "listProjectDir")).toBe(true),
    );
    deliverListDir(posted, deliver, "", [
      {
        name: "A.lua",
        path: "behaviors/A.lua",
        isDirectory: false,
        size: 1,
      },
      {
        name: "B.lua",
        path: "behaviors/B.lua",
        isDirectory: false,
        size: 1,
      },
    ]);
    await screen.findByText("A");
    doubleClickAssetStem("A");
    await waitFor(() =>
      expect(posted.some((m) => m.op === "readProjectFile")).toBe(true),
    );
    const ar = posted.filter((m) => m.op === "readProjectFile").pop();
    await act(async () => {
      deliver({
        requestId: String(ar?.requestId ?? ""),
        ok: true,
        content: "alpha",
      });
    });
    doubleClickAssetStem("B");
    await waitFor(() =>
      expect(posted.filter((m) => m.op === "readProjectFile")).toHaveLength(
        2,
      ),
    );
    const br = posted.filter((m) => m.op === "readProjectFile").pop();
    await act(async () => {
      deliver({
        requestId: String(br?.requestId ?? ""),
        ok: true,
        content: "beta",
      });
    });
    await user.click(screen.getByRole("tab", { name: /A\.lua/ }));
    fireEvent.change(screen.getByTestId("monaco-stub"), {
      target: { value: "alpha-edited" },
    });
    await user.click(screen.getByRole("tab", { name: /B\.lua/ }));
    expect(screen.getByTestId("monaco-stub")).toHaveValue("beta");
    await user.click(screen.getByRole("tab", { name: /A\.lua/ }));
    expect(screen.getByTestId("monaco-stub")).toHaveValue("alpha-edited");
  });

  it("closing a tab removes it", async () => {
    const user = userEvent.setup();
    const { posted, deliver } = captureEngineBridgePosts();
    render(<EditorWithAssetBrowser />);
    await waitFor(() =>
      expect(posted.some((m) => m.op === "listProjectDir")).toBe(true),
    );
    deliverListDir(posted, deliver, "", [
      {
        name: "Ship.lua",
        path: "behaviors/Ship.lua",
        isDirectory: false,
        size: 1,
      },
    ]);
    await screen.findByText("Ship");
    doubleClickAssetStem("Ship");
    await waitFor(() =>
      expect(posted.some((m) => m.op === "readProjectFile")).toBe(true),
    );
    const r0 = posted.filter((m) => m.op === "readProjectFile").pop();
    await act(async () => {
      deliver({
        requestId: String(r0?.requestId ?? ""),
        ok: true,
        content: "x",
      });
    });
    await waitFor(() => expect(screen.getAllByRole("tab")).toHaveLength(1));
    await user.click(
      screen.getByRole("button", { name: /Close behaviors\/Ship\.lua/i }),
    );
    expect(screen.queryByRole("tab")).toBeNull();
  });

  it("openFileRequest with content focuses tab without readProjectFile", async () => {
    const { posted } = captureEngineBridgePosts();
    const onConsumed = vi.fn();
    render(
      <LuaEditorPanel
        openFileRequest={{
          path: "behaviors/New.lua",
          content: "-- fresh",
          id: 1,
        }}
        onConsumedOpenFileRequest={onConsumed}
      />,
    );
    await waitFor(() =>
      expect(screen.getByTestId("monaco-stub")).toHaveValue("-- fresh"),
    );
    expect(onConsumed).toHaveBeenCalled();
    expect(posted.some((m) => m.op === "readProjectFile")).toBe(false);
  });
});
