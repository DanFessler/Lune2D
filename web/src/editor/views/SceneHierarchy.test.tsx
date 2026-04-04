import { render, screen } from "@testing-library/react";
import { afterEach, describe, expect, it } from "vitest";
import SceneHierarchy from "./SceneHierarchy";
import { resetEngineScriptBridgeForTests } from "../../luaEditorBridge";

afterEach(() => {
  resetEngineScriptBridgeForTests();
});

describe("SceneHierarchy", () => {
  it("renders hierarchy search", () => {
    render(
      <SceneHierarchy entities={[]} selectedId={null} onSelect={() => {}} />,
    );
    expect(screen.getByLabelText("Search hierarchy")).toBeInTheDocument();
  });
});
