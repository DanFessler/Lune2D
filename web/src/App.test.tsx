import { act, fireEvent, render, screen, waitFor } from "@testing-library/react";
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

      await waitFor(() =>
        expect(posted.some((m) => m.op === "readSharedSettingsFile")).toBe(
          true,
        ),
      );
      const dockLayoutMsg = posted.find((m) => m.op === "readSharedSettingsFile");
      await act(async () => {
        deliver({
          requestId: String(dockLayoutMsg?.requestId ?? ""),
          ok: false,
          error: "not found",
        });
      });

      expect(screen.getByTitle("Lua")).toBeInTheDocument();
      expect(screen.getByTitle("Assets")).toBeInTheDocument();
      expect(screen.getByTitle("Game")).toBeInTheDocument();
      expect(screen.getByTitle("Hierarchy")).toBeInTheDocument();
      expect(screen.getByTitle("Inspector")).toBeInTheDocument();

      await waitFor(() =>
        expect(posted.some((m) => m.op === "listProjectDir")).toBe(true),
      );
      const listMsg = posted.find((m) => m.op === "listProjectDir");
      await act(async () => {
        deliver({
          requestId: String(listMsg?.requestId ?? ""),
          ok: true,
          entries: [
            {
              name: "game.lua",
              path: "game.lua",
              isDirectory: false,
              size: 4,
            },
          ],
        });
      });
      await screen.findByText("game");
      const luaLabel = screen.getByText("game");
      fireEvent.doubleClick(luaLabel.parentElement!);
      await waitFor(() =>
        expect(posted.some((m) => m.op === "readProjectFile")).toBe(true),
      );
      const readMsg = posted.find((m) => m.op === "readProjectFile");
      await act(async () => {
        deliver({
          requestId: String(readMsg?.requestId ?? ""),
          ok: true,
          content: "-- ok",
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
