import { act, fireEvent, render, screen, waitFor } from "@testing-library/react";
import userEvent from "@testing-library/user-event";
import { afterEach, describe, expect, it, vi } from "vitest";
import { LuaEditorPanel } from "./LuaEditorPanel";
import { captureEngineBridgePosts } from "./test/engineBridgeTestUtils";
import { resetEngineScriptBridgeForTests } from "./luaEditorBridge";

afterEach(() => {
  resetEngineScriptBridgeForTests();
  vi.clearAllMocks();
});

async function flushListLua(
  posted: Record<string, unknown>[],
  deliver: (r: import("./luaEditorBridge").EngineScriptResponse) => void,
) {
  const msg = posted.find((m) => m.op === "listLua");
  if (!msg) return;
  deliver({
    requestId: String(msg.requestId ?? ""),
    ok: true,
    files: [{ path: "behaviors/Ship.lua", content: "-- initial" }],
  });
}

describe("LuaEditorPanel", () => {
  it("loads workspace Lua files and shows the first in the editor", async () => {
    const { posted, deliver } = captureEngineBridgePosts();
    render(<LuaEditorPanel />);
    await waitFor(() => expect(posted.some((m) => m.op === "listLua")).toBe(true));
    await flushListLua(posted, deliver);
    await waitFor(() =>
      expect(screen.getByTestId("monaco-stub")).toHaveValue("-- initial"),
    );
  });

  it("Save writes the current buffer via the bridge", async () => {
    const user = userEvent.setup();
    const { posted, deliver } = captureEngineBridgePosts();
    render(<LuaEditorPanel />);
    await waitFor(() => expect(posted.some((m) => m.op === "listLua")).toBe(true));
    await flushListLua(posted, deliver);
    await waitFor(() => screen.findByTestId("monaco-stub"));
    fireEvent.change(screen.getByTestId("monaco-stub"), {
      target: { value: "updated" },
    });
    await user.click(screen.getByRole("button", { name: /save/i }));

    await waitFor(() => {
      const w = posted.find((m) => m.op === "writeLua");
      expect(w?.content).toBe("updated");
      expect(w?.path).toBe("behaviors/Ship.lua");
    });
    const wmsg = posted.find((m) => m.op === "writeLua");
    await act(async () => {
      deliver({ requestId: String(wmsg?.requestId ?? ""), ok: true });
    });
  });

  it("shows load error when listLua fails", async () => {
    const { posted, deliver } = captureEngineBridgePosts();
    render(<LuaEditorPanel />);
    await waitFor(() => expect(posted.some((m) => m.op === "listLua")).toBe(true));
    const msg = posted.find((m) => m.op === "listLua");
    await act(async () => {
      deliver({
        requestId: String(msg?.requestId ?? ""),
        ok: false,
        error: "Lua workspace not configured",
      });
    });
    await waitFor(() =>
      expect(screen.getByText(/Lua workspace not configured/)).toBeInTheDocument(),
    );
  });

  it("Save triggers a second listLua refresh after a successful write", async () => {
    const user = userEvent.setup();
    const { posted, deliver } = captureEngineBridgePosts();
    render(<LuaEditorPanel />);
    await waitFor(() => expect(posted.some((m) => m.op === "listLua")).toBe(true));
    const list0 = posted.find((m) => m.op === "listLua");
    await act(async () => {
      deliver({
        requestId: String(list0?.requestId ?? ""),
        ok: true,
        files: [{ path: "behaviors/Ship.lua", content: "v1" }],
      });
    });
    await waitFor(() =>
      expect(screen.getByTestId("monaco-stub")).toHaveValue("v1"),
    );
    fireEvent.change(screen.getByTestId("monaco-stub"), {
      target: { value: "v2" },
    });
    await user.click(screen.getByRole("button", { name: /save/i }));
    await waitFor(() => expect(posted.some((m) => m.op === "writeLua")).toBe(true));
    const w = posted.find((m) => m.op === "writeLua");
    await act(async () => {
      deliver({ requestId: String(w?.requestId ?? ""), ok: true });
    });
    await waitFor(() => expect(posted.filter((m) => m.op === "listLua").length).toBe(2));
    const list1 = posted.filter((m) => m.op === "listLua").pop();
    await act(async () => {
      deliver({
        requestId: String(list1?.requestId ?? ""),
        ok: true,
        files: [{ path: "behaviors/Ship.lua", content: "v2" }],
      });
    });
    await waitFor(() =>
      expect(screen.getByTestId("monaco-stub")).toHaveValue("v2"),
    );
  });
});
