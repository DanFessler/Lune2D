import { describe, expect, it } from "vitest";
import { isSceneAssetPath } from "./sceneAssetPath";

describe("isSceneAssetPath", () => {
  it("is true for *.scene.json under scenes/", () => {
    expect(isSceneAssetPath("scenes/foo.scene.json")).toBe(true);
  });

  it("is true for *.scene", () => {
    expect(isSceneAssetPath("scenes/foo.scene")).toBe(true);
  });

  it("is false for plain scenes/*.json", () => {
    expect(isSceneAssetPath("scenes/foo.json")).toBe(false);
  });

  it("is false for root config.json", () => {
    expect(isSceneAssetPath("config.json")).toBe(false);
  });

  it("is false for Lua", () => {
    expect(isSceneAssetPath("behaviors/x.lua")).toBe(false);
  });
});
