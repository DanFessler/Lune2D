import { act, render, screen, waitFor } from "@testing-library/react";
import { afterEach, describe, expect, it, vi } from "vitest";
import App from "./App";
import { captureEngineBridgePosts } from "./test/engineBridgeTestUtils";
import { resetEngineScriptBridgeForTests } from "./luaEditorBridge";

afterEach(() => {
  resetEngineScriptBridgeForTests();
});

describe("App dock layout", () => {
  it("includes a Lua dock tab wired to the Lua editor stack", async () => {
    const log = vi.spyOn(console, "log").mockImplementation(() => {});
    try {
      const { posted, deliver } = captureEngineBridgePosts();
      render(<App />);

      expect(screen.getByTitle("Lua")).toBeInTheDocument();
      expect(screen.getByTitle("Game")).toBeInTheDocument();
      expect(screen.getByTitle("Entities")).toBeInTheDocument();
      expect(screen.getByTitle("Inspector")).toBeInTheDocument();

      await waitFor(() =>
        expect(posted.some((m) => m.op === "listLua")).toBe(true),
      );
      const msg = posted.find((m) => m.op === "listLua");
      await act(async () => {
        deliver({
          requestId: String(msg?.requestId ?? ""),
          ok: true,
          files: [{ path: "game.lua", content: "-- ok" }],
        });
      });
      await waitFor(() =>
        expect(screen.getByTestId("monaco-stub")).toHaveValue("-- ok"),
      );
    } finally {
      log.mockRestore();
    }
  });
});
