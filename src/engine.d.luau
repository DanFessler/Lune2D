--!strict
-- Type definitions for the C++ engine API.
-- Used by luau-analyze for static checking; not loaded at runtime.

export type Point   = { number }   -- {x, y} positional pair
export type Polygon = { Point }

-- w/h match #game-surface layout size when inset is active (e.g. 756×532), else full window logical.
declare screen: { w: number, h: number }

declare draw: {
    clear:   (r: number, g: number, b: number) -> (),
    line:    (x1: number, y1: number, x2: number, y2: number, r: number, g: number, b: number, a: number) -> (),
    circle:  (x: number, y: number, radius: number, r: number, g: number, b: number, a: number) -> (),
    poly:    (verts: Polygon, x: number, y: number, angle: number, r: number, g: number, b: number, a: number) -> (),
    char:    (c: string, x: number, y: number, scale: number, r: number, g: number, b: number, a: number) -> (),
    number:  (n: number, x: number, y: number, scale: number, r: number, g: number, b: number, a: number) -> (),
    present: () -> (),
}

declare input: {
    down: (key: string) -> boolean,
}

declare audio: {
    play:   (name: string) -> (),
    thrust: (on: boolean) -> (),
}

declare app: {
    quit: () -> (),
}
