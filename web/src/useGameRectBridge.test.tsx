import { act, render, waitFor } from "@testing-library/react";
import { useLayoutEffect, useState } from "react";
import { afterEach, beforeEach, describe, expect, it, vi } from "vitest";
import { useGameRectBridge } from "./useGameRectBridge";

function Harness() {
  const [surface, setSurface] = useState<HTMLDivElement | null>(null);

  useLayoutEffect(() => {
    if (!surface) return;
    surface.getBoundingClientRect = () => new DOMRect(100, 80, 400, 300);
  }, [surface]);

  useGameRectBridge(surface);
  return <div ref={setSurface} />;
}

describe("useGameRectBridge", () => {
  const postMessage = vi.fn();

  beforeEach(() => {
    postMessage.mockReset();
    window.webkit = {
      messageHandlers: {
        gameRect: { postMessage },
      },
    };
    Object.defineProperty(document.documentElement, "clientWidth", {
      configurable: true,
      value: 800,
    });
    Object.defineProperty(document.documentElement, "clientHeight", {
      configurable: true,
      value: 600,
    });
  });

  afterEach(() => {
    document.body.innerHTML = "";
  });

  it("posts the surface rect when no overlay blocks the viewport", async () => {
    render(<Harness />);

    await waitFor(() =>
      expect(postMessage).toHaveBeenCalledWith({
        x: 100,
        y: 80,
        w: 400,
        h: 300,
        uiw: 800,
        uih: 600,
      }),
    );
  });

  it("clears and restores passthrough for overlapping dialogs", async () => {
    render(<Harness />);

    await waitFor(() =>
      expect(postMessage).toHaveBeenCalledWith({
        x: 100,
        y: 80,
        w: 400,
        h: 300,
        uiw: 800,
        uih: 600,
      }),
    );

    const dialog = document.createElement("div");
    dialog.setAttribute("role", "dialog");
    dialog.getBoundingClientRect = () => new DOMRect(150, 120, 160, 100);

    await act(async () => {
      document.body.appendChild(dialog);
    });

    await waitFor(() =>
      expect(postMessage).toHaveBeenLastCalledWith({
        x: 0,
        y: 0,
        w: 0,
        h: 0,
        uiw: 800,
        uih: 600,
      }),
    );

    await act(async () => {
      dialog.remove();
    });

    await waitFor(() =>
      expect(postMessage).toHaveBeenLastCalledWith({
        x: 100,
        y: 80,
        w: 400,
        h: 300,
        uiw: 800,
        uih: 600,
      }),
    );
  });
});
