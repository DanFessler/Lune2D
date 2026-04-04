import { act, render, screen, waitFor } from "@testing-library/react";
import userEvent from "@testing-library/user-event";
import { afterEach, describe, expect, it, vi } from "vitest";
import SceneHierarchy from "./SceneHierarchy";
import { captureEngineBridgePosts } from "../../test/engineBridgeTestUtils";
import { resetEngineScriptBridgeForTests } from "../../luaEditorBridge";

afterEach(() => {
  resetEngineScriptBridgeForTests();
});

describe("SceneHierarchy", () => {
  it("New entity sends runtime.spawn with default name and selects the returned id", async () => {
    const user = userEvent.setup();
    const onSelect = vi.fn();
    const { posted, deliver } = captureEngineBridgePosts();
    render(
      <SceneHierarchy entities={[]} selectedId={null} onSelect={onSelect} />,
    );

    await user.click(screen.getByRole("button", { name: /new entity/i }));

    await waitFor(() =>
      expect(posted.some((m) => m.op === "runtime.spawn")).toBe(true),
    );
    const msg = posted.find((m) => m.op === "runtime.spawn");
    expect(msg?.args).toEqual(["Entity"]);

    await act(async () => {
      deliver({
        requestId: String(msg?.requestId ?? ""),
        ok: true,
        result: 42,
      });
    });

    await waitFor(() => expect(onSelect).toHaveBeenCalledWith("42"));
  });
});
