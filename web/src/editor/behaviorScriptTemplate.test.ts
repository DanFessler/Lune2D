import { describe, expect, it } from "vitest";
import {
  sanitizeBehaviorBaseName,
  editorBehaviorLifecycleTemplate,
  editorBehaviorRelativePath,
} from "./behaviorScriptTemplate";

describe("sanitizeBehaviorBaseName", () => {
  it("accepts PascalCase and snake_case", () => {
    expect(sanitizeBehaviorBaseName("CoinPickup")).toBe("CoinPickup");
    expect(sanitizeBehaviorBaseName("my_behavior")).toBe("my_behavior");
    expect(sanitizeBehaviorBaseName("  Trimmed  ")).toBe("Trimmed");
  });

  it("rejects empty and invalid tokens", () => {
    expect(sanitizeBehaviorBaseName("")).toBeNull();
    expect(sanitizeBehaviorBaseName("   ")).toBeNull();
    expect(sanitizeBehaviorBaseName("bad-name")).toBeNull();
    expect(sanitizeBehaviorBaseName("1StartsWithDigit")).toBeNull();
    expect(sanitizeBehaviorBaseName("has space")).toBeNull();
  });
});

describe("editorBehaviorRelativePath", () => {
  it("returns editor/<name>.lua", () => {
    expect(editorBehaviorRelativePath("Ship")).toBe("editor/Ship.lua");
  });
});

describe("editorBehaviorLifecycleTemplate", () => {
  it("references the paired runtime behavior", () => {
    const tmpl = editorBehaviorLifecycleTemplate("Ship");
    expect(tmpl).toContain("behaviors/Ship.lua");
    expect(tmpl).toContain("--!strict");
    expect(tmpl).toContain("updateWorld");
    expect(tmpl).toContain("drawWorld");
  });
});
