// TDD tests for Phase 4: Inspector pairing UX.
// Validates that behaviors render uniformly with engine/user distinction and pairing indicators.

import { fireEvent, render, screen } from "@testing-library/react";
import { beforeEach, describe, expect, it, vi } from "vitest";
import EngineInspector from "./EngineInspector";
import type { EngineEntity, BehaviorComponent } from "../../engineBridge";
import { engine } from "../../engineProxy";

vi.mock("../../engineProxy", () => ({
  engine: {
    runtime: {
      setName: vi.fn(),
      setActive: vi.fn(),
      removeScript: vi.fn(),
      reorderScript: vi.fn(),
      setScriptProperty: vi.fn(),
      setBehaviorProperty: vi.fn(),
      removeBehavior: vi.fn(),
      reorderBehavior: vi.fn(),
    },
  },
}));

function makeBehavior(
  name: string,
  opts: Partial<BehaviorComponent> = {},
): BehaviorComponent {
  return {
    type: "Behavior",
    name,
    isNative: false,
    ...opts,
  };
}

function makeEntity(
  id: string,
  behaviors: BehaviorComponent[],
): EngineEntity {
  return {
    id,
    name: `Entity_${id}`,
    active: true,
    drawOrder: 0,
    updateOrder: 0,
    components: behaviors,
  };
}

describe("EngineInspector - unified behavior model", () => {
  beforeEach(() => {
    vi.clearAllMocks();
  });

  it("renders Transform as a behavior entry (not a special component)", () => {
    const entity = makeEntity("1", [
      makeBehavior("Transform", {
        isNative: true,
        propertyValues: { x: 10, y: 20, angle: 0, vx: 0, vy: 0, sx: 1, sy: 1 },
        propertySchema: [
          { name: "x", type: "number" },
          { name: "y", type: "number" },
          { name: "angle", type: "number" },
          { name: "vx", type: "number" },
          { name: "vy", type: "number" },
        ],
      }),
    ]);

    render(<EngineInspector entity={entity} />);
    expect(screen.getByText("Transform")).toBeInTheDocument();
  });

  it("renders engine behaviors with visual distinction from user behaviors", () => {
    const entity = makeEntity("1", [
      makeBehavior("Transform", { isNative: true }),
      makeBehavior("Ship", { isNative: false }),
    ]);

    render(<EngineInspector entity={entity} />);
    const transformLabel = screen.getByText("Transform");
    const shipLabel = screen.getByText("Ship");
    expect(transformLabel).toBeInTheDocument();
    expect(shipLabel).toBeInTheDocument();

    // Engine behaviors should have a distinguishing indicator.
    // Checking for a data attribute or aria label.
    const transformBlock = transformLabel.closest("[data-behavior-kind]");
    const shipBlock = shipLabel.closest("[data-behavior-kind]");
    expect(transformBlock?.getAttribute("data-behavior-kind")).toBe("engine");
    expect(shipBlock?.getAttribute("data-behavior-kind")).toBe("user");
  });

  it("shows pairing indicator for behaviors with editor modules", () => {
    const entity = makeEntity("1", [
      makeBehavior("Transform", { isNative: true, hasEditorPair: true }),
      makeBehavior("Ship", { isNative: false, hasEditorPair: true }),
      makeBehavior("Collider", { isNative: false, hasEditorPair: false }),
    ]);

    render(<EngineInspector entity={entity} />);

    const transformBlock = screen.getByText("Transform").closest("[data-has-editor-pair]");
    const shipBlock = screen.getByText("Ship").closest("[data-has-editor-pair]");
    const colliderBlock = screen.getByText("Collider").closest("[data-has-editor-pair]");

    expect(transformBlock?.getAttribute("data-has-editor-pair")).toBe("true");
    expect(shipBlock?.getAttribute("data-has-editor-pair")).toBe("true");
    expect(colliderBlock?.getAttribute("data-has-editor-pair")).toBe("false");
  });

  it("does not show remove button for non-removable engine behaviors", () => {
    const entity = makeEntity("1", [
      makeBehavior("Transform", { isNative: true }),
      makeBehavior("Ship", { isNative: false }),
    ]);

    render(<EngineInspector entity={entity} />);

    // User behaviors should have remove button, engine behaviors should not.
    const removeButtons = screen.getAllByRole("button", { name: /Remove/ });
    expect(removeButtons.length).toBe(1);
    expect(removeButtons[0].getAttribute("aria-label")).toBe("Remove Ship");
  });

  it("renders vector properties as two numeric inputs and commits arrays", () => {
    const entity = makeEntity("7", [
      makeBehavior("Sprite", {
        propertyValues: { origin: [0.25, 0.75] },
        propertySchema: [{ name: "origin", type: "vector", default: [0.5, 0.5] }],
      }),
    ]);

    render(<EngineInspector entity={entity} />);

    const xInput = screen.getByLabelText("origin X");
    const yInput = screen.getByLabelText("origin Y");
    expect(xInput).toHaveValue(0.25);
    expect(yInput).toHaveValue(0.75);

    fireEvent.change(xInput, { target: { value: "1.5" } });
    fireEvent.blur(xInput);

    expect(engine.runtime.setBehaviorProperty).toHaveBeenCalledWith(
      7,
      0,
      "origin",
      [1.5, 0.75],
    );
  });

  it("renders transform vector groups through the shared inspector pipeline", () => {
    const entity = makeEntity("3", [
      makeBehavior("Transform", {
        isNative: true,
        propertyValues: {
          position: [10, 20],
          angle: 45,
          velocity: [1, 2],
          scale: [3, 4],
        },
        propertySchema: [
          { name: "position", type: "vector", default: [0, 0] },
          { name: "angle", type: "number" },
          { name: "velocity", type: "vector", default: [0, 0] },
          { name: "scale", type: "vector", default: [1, 1] },
        ],
      }),
    ]);

    render(<EngineInspector entity={entity} />);

    const positionX = screen.getByLabelText("position X");
    expect(screen.getByLabelText("position Y")).toHaveValue(20);
    expect(screen.getByLabelText("velocity X")).toHaveValue(1);
    expect(screen.getByLabelText("scale Y")).toHaveValue(4);

    fireEvent.change(positionX, { target: { value: "12.5" } });
    fireEvent.blur(positionX);

    expect(engine.runtime.setBehaviorProperty).toHaveBeenCalledWith(
      3,
      0,
      "position",
      [12.5, 20],
    );
  });
});
