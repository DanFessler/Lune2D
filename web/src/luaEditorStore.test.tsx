import { act, render, screen } from "@testing-library/react";
import { afterEach, describe, expect, it } from "vitest";
import { LuaEditorPanel } from "./LuaEditorPanel";
import { resetEngineScriptBridgeForTests } from "./luaEditorBridge";
import { useLuaEditorStore } from "./luaEditorStore";
import { captureEngineBridgePosts } from "./test/engineBridgeTestUtils";

afterEach(() => {
  resetEngineScriptBridgeForTests();
});

describe("luaEditorStore", () => {
  it("keeps tabs and active file when LuaEditorPanel unmounts and remounts", () => {
    captureEngineBridgePosts();
    const { unmount } = render(<LuaEditorPanel />);
    act(() => {
      useLuaEditorStore.getState().upsertTab("behaviors/X.lua", "-- hi", false);
    });
    expect(screen.getByRole("tab", { name: /X\.lua/ })).toBeInTheDocument();
    unmount();

    render(<LuaEditorPanel />);
    expect(useLuaEditorStore.getState().tabs).toHaveLength(1);
    expect(useLuaEditorStore.getState().activePath).toBe("behaviors/X.lua");
    expect(screen.getByTestId("monaco-stub")).toHaveValue("-- hi");
  });
});
