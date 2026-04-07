import { act, fireEvent, render, screen, waitFor } from "@testing-library/react";
import { afterEach, describe, expect, it, vi } from "vitest";
import AssetBrowser from "./AssetBrowser";
import { captureEngineBridgePosts } from "../../test/engineBridgeTestUtils";
import { resetEngineScriptBridgeForTests } from "../../luaEditorBridge";

afterEach(() => {
  resetEngineScriptBridgeForTests();
  vi.clearAllMocks();
});

function deliverListDir(
  posted: Record<string, unknown>[],
  deliver: (r: import("../../luaEditorBridge").EngineScriptResponse) => void,
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

function doubleClickAssetStem(stem: string) {
  const label = screen.getByText(stem);
  const tile = label.parentElement;
  expect(tile).toBeTruthy();
  fireEvent.doubleClick(tile!);
}

describe("AssetBrowser", () => {
  it("lists project root on mount (listProjectDir dir \"\")", async () => {
    const { posted } = captureEngineBridgePosts();
    render(
      <AssetBrowser onOpenEntry={() => {}} onLoadScene={() => {}} />,
    );
    await waitFor(() => {
      const msg = posted.find((m) => m.op === "listProjectDir");
      expect(msg?.dir).toBe("");
    });
  });

  it("shows error when listProjectDir fails", async () => {
    const { posted, deliver } = captureEngineBridgePosts();
    render(
      <AssetBrowser onOpenEntry={() => {}} onLoadScene={() => {}} />,
    );
    await waitFor(() =>
      expect(posted.some((m) => m.op === "listProjectDir")).toBe(true),
    );
    const msg = posted.find((m) => m.op === "listProjectDir");
    await act(async () => {
      deliver({
        requestId: String(msg?.requestId ?? ""),
        ok: false,
        error: "Project workspace not configured",
      });
    });
    await waitFor(() =>
      expect(
        screen.getByText(/Project workspace not configured/),
      ).toBeInTheDocument(),
    );
  });

  it("navigating into a folder requests listProjectDir with that relative dir", async () => {
    const { posted, deliver } = captureEngineBridgePosts();
    render(
      <AssetBrowser onOpenEntry={() => {}} onLoadScene={() => {}} />,
    );
    await waitFor(() =>
      expect(posted.some((m) => m.op === "listProjectDir")).toBe(true),
    );
    deliverListDir(posted, deliver, "", [
      {
        name: "behaviors",
        path: "behaviors",
        isDirectory: true,
        size: 0,
      },
    ]);
    await screen.findByText("behaviors");
    doubleClickAssetStem("behaviors");
    await waitFor(() =>
      expect(
        posted.some(
          (m) => m.op === "listProjectDir" && m.dir === "behaviors",
        ),
      ).toBe(true),
    );
  });

  it("root crumb returns to project root (empty dir segment)", async () => {
    const { posted, deliver } = captureEngineBridgePosts();
    render(
      <AssetBrowser onOpenEntry={() => {}} onLoadScene={() => {}} />,
    );
    await waitFor(() =>
      expect(posted.some((m) => m.op === "listProjectDir")).toBe(true),
    );
    const first = posted.find((m) => m.op === "listProjectDir");
    await act(async () => {
      deliver({
        requestId: String(first?.requestId ?? ""),
        ok: true,
        entries: [
          {
            name: "behaviors",
            path: "behaviors",
            isDirectory: true,
            size: 0,
          },
        ],
      });
    });
    await screen.findByText("behaviors");
    doubleClickAssetStem("behaviors");
    await waitFor(() =>
      expect(posted.filter((m) => m.op === "listProjectDir").length).toBe(2),
    );
    const second = posted.filter((m) => m.op === "listProjectDir").pop();
    await act(async () => {
      deliver({
        requestId: String(second?.requestId ?? ""),
        ok: true,
        entries: [],
      });
    });
    await screen.findByText("project");
    fireEvent.click(screen.getByText("project"));
    await waitFor(() =>
      expect(posted.filter((m) => m.op === "listProjectDir").length).toBe(3),
    );
    const third = posted.filter((m) => m.op === "listProjectDir").pop();
    expect(String(third?.dir ?? "")).toBe("");
  });

  it("invokes onOpenEntry when double-clicking a non-scene file", async () => {
    const onOpenEntry = vi.fn();
    const { posted, deliver } = captureEngineBridgePosts();
    render(
      <AssetBrowser onOpenEntry={onOpenEntry} onLoadScene={() => {}} />,
    );
    await waitFor(() =>
      expect(posted.some((m) => m.op === "listProjectDir")).toBe(true),
    );
    deliverListDir(posted, deliver, "", [
      {
        name: "Tool.lua",
        path: "behaviors/Tool.lua",
        isDirectory: false,
        size: 1,
      },
    ]);
    await screen.findByText("Tool");
    doubleClickAssetStem("Tool");
    await waitFor(() =>
      expect(onOpenEntry).toHaveBeenCalledWith("behaviors/Tool.lua"),
    );
  });

  it("invokes onLoadScene for scene asset path", async () => {
    const onLoadScene = vi.fn();
    const { posted, deliver } = captureEngineBridgePosts();
    render(
      <AssetBrowser onOpenEntry={() => {}} onLoadScene={onLoadScene} />,
    );
    await waitFor(() =>
      expect(posted.some((m) => m.op === "listProjectDir")).toBe(true),
    );
    deliverListDir(posted, deliver, "", [
      {
        name: "main.scene.json",
        path: "scenes/main.scene.json",
        isDirectory: false,
        size: 1,
      },
    ]);
    await screen.findByText("main");
    doubleClickAssetStem("main");
    await waitFor(() =>
      expect(onLoadScene).toHaveBeenCalledWith("scenes/main.scene.json"),
    );
  });

  it("re-fetches when refreshKey changes", async () => {
    const { posted, deliver } = captureEngineBridgePosts();
    const { rerender } = render(
      <AssetBrowser
        refreshKey={0}
        onOpenEntry={() => {}}
        onLoadScene={() => {}}
      />,
    );
    await waitFor(() =>
      expect(posted.some((m) => m.op === "listProjectDir")).toBe(true),
    );
    const first = posted.find((m) => m.op === "listProjectDir");
    await act(async () => {
      deliver({
        requestId: String(first?.requestId ?? ""),
        ok: true,
        entries: [],
      });
    });
    rerender(
      <AssetBrowser
        refreshKey={1}
        onOpenEntry={() => {}}
        onLoadScene={() => {}}
      />,
    );
    await waitFor(() =>
      expect(posted.filter((m) => m.op === "listProjectDir").length).toBe(2),
    );
  });
});
