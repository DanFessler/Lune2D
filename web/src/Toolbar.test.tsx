import { act, render, screen, waitFor } from "@testing-library/react";
import userEvent from "@testing-library/user-event";
import { afterEach, describe, expect, it, vi } from "vitest";
import { Toolbar } from "./Toolbar";
import { captureEngineBridgePosts } from "./test/engineBridgeTestUtils";
import { resetEngineScriptBridgeForTests } from "./luaEditorBridge";

afterEach(() => {
  resetEngineScriptBridgeForTests();
  vi.clearAllMocks();
});

describe("Toolbar", () => {
  it("renders Play, Pause, and Stop buttons", () => {
    captureEngineBridgePosts();
    render(<Toolbar />);
    expect(screen.getByRole("button", { name: /play/i })).toBeInTheDocument();
    expect(screen.getByRole("button", { name: /pause/i })).toBeInTheDocument();
    expect(screen.getByRole("button", { name: /stop/i })).toBeInTheDocument();
  });

  it("Play from stopped triggers startSimulation with captureEditorSnapshot true", async () => {
    const user = userEvent.setup();
    const { posted, deliver } = captureEngineBridgePosts();
    render(<Toolbar />);
    await user.click(screen.getByRole("button", { name: /play/i }));
    await waitFor(() =>
      expect(posted.some((m) => m.op === "startSimulation")).toBe(true),
    );
    const msg = posted.find((m) => m.op === "startSimulation");
    expect(msg?.captureEditorSnapshot).toBe(true);
    await act(async () => {
      deliver({ requestId: String(msg?.requestId ?? ""), ok: true });
    });
  });

  it("Play from paused triggers startSimulation with captureEditorSnapshot false", async () => {
    const user = userEvent.setup();
    const { posted, deliver } = captureEngineBridgePosts();
    render(<Toolbar />);

    await user.click(screen.getByRole("button", { name: /play/i }));
    await waitFor(() =>
      expect(posted.some((m) => m.op === "startSimulation")).toBe(true),
    );
    const firstStart = posted.find((m) => m.op === "startSimulation");
    await act(async () => {
      deliver({ requestId: String(firstStart?.requestId ?? ""), ok: true });
    });
    await waitFor(() =>
      expect(screen.getByRole("button", { name: /pause/i })).not.toBeDisabled(),
    );

    await user.click(screen.getByRole("button", { name: /pause/i }));
    await waitFor(() => expect(posted.some((m) => m.op === "setPaused")).toBe(true));
    const pauseMsg = posted.find((m) => m.op === "setPaused");
    await act(async () => {
      deliver({ requestId: String(pauseMsg?.requestId ?? ""), ok: true });
    });

    await user.click(screen.getByRole("button", { name: /play/i }));
    await waitFor(() =>
      expect(
        posted.some((m) => m.op === "setPaused" && m.paused === false),
      ).toBe(true),
    );
    const unpause = posted.find((m) => m.op === "setPaused" && m.paused === false);
    await act(async () => {
      deliver({ requestId: String(unpause?.requestId ?? ""), ok: true });
    });
    await waitFor(() => {
      const starts = posted.filter((m) => m.op === "startSimulation");
      expect(starts.length).toBeGreaterThanOrEqual(2);
      expect(starts[starts.length - 1]?.captureEditorSnapshot).toBe(false);
    });
    const lastStart = posted.filter((m) => m.op === "startSimulation").pop();
    await act(async () => {
      deliver({ requestId: String(lastStart?.requestId ?? ""), ok: true });
    });
  });

  it("Pause triggers setPaused", async () => {
    const user = userEvent.setup();
    const { posted, deliver } = captureEngineBridgePosts();
    render(<Toolbar />);

    // First play so pause is enabled
    await user.click(screen.getByRole("button", { name: /play/i }));
    await waitFor(() =>
      expect(posted.some((m) => m.op === "startSimulation")).toBe(true),
    );
    const startMsg = posted.find((m) => m.op === "startSimulation");
    await act(async () => {
      deliver({ requestId: String(startMsg?.requestId ?? ""), ok: true });
    });

    await user.click(screen.getByRole("button", { name: /pause/i }));
    await waitFor(() => {
      const msg = posted.find((m) => m.op === "setPaused");
      expect(msg?.paused).toBe(true);
    });
    const pauseMsg = posted.find((m) => m.op === "setPaused");
    await act(async () => {
      deliver({ requestId: String(pauseMsg?.requestId ?? ""), ok: true });
    });
  });

  it("Stop triggers restartGame", async () => {
    const user = userEvent.setup();
    const { posted, deliver } = captureEngineBridgePosts();
    render(<Toolbar />);

    // Play first so stop is enabled
    await user.click(screen.getByRole("button", { name: /play/i }));
    await waitFor(() =>
      expect(posted.some((m) => m.op === "startSimulation")).toBe(true),
    );
    const startMsg = posted.find((m) => m.op === "startSimulation");
    await act(async () => {
      deliver({ requestId: String(startMsg?.requestId ?? ""), ok: true });
    });

    await user.click(screen.getByRole("button", { name: /stop/i }));
    await waitFor(() =>
      expect(posted.some((m) => m.op === "restartGame")).toBe(true),
    );
    const stopMsg = posted.find((m) => m.op === "restartGame");
    await act(async () => {
      deliver({ requestId: String(stopMsg?.requestId ?? ""), ok: true });
    });
  });

  it("Pause is disabled when not playing", () => {
    captureEngineBridgePosts();
    render(<Toolbar />);
    expect(screen.getByRole("button", { name: /pause/i })).toBeDisabled();
  });

  it("Stop is disabled when already stopped", () => {
    captureEngineBridgePosts();
    render(<Toolbar />);
    expect(screen.getByRole("button", { name: /stop/i })).toBeDisabled();
  });
});
