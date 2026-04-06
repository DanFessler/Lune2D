--!strict
-- Type definitions for native bindings (luau-analyze only).

export type Point = { number }
export type Polygon = { Point }

-- Proxied by C++; behaves like a mutable table with x, y, angle, vx, vy, sx, sy.
export type TransformHandle = {
	x: number,
	y: number,
	angle: number,
	vx: number,
	vy: number,
	sx: number,
	sy: number,
}

export type WorldTransform = {
	x: number,
	y: number,
	angle: number,
	sx: number,
	sy: number,
}

declare screen: { w: number, h: number }

declare draw: {
	clear: (r: number, g: number, b: number) -> (),
	line: (x1: number, y1: number, x2: number, y2: number, r: number, g: number, b: number, a: number) -> (),
	circle: (x: number, y: number, radius: number, r: number, g: number, b: number, a: number) -> (),
	poly: (verts: Polygon, x: number, y: number, angle: number, r: number, g: number, b: number, a: number) -> (),
	char: (c: string, x: number, y: number, scale: number, r: number, g: number, b: number, a: number) -> (),
	number: (n: number, x: number, y: number, scale: number, r: number, g: number, b: number, a: number) -> (),
	present: () -> (),
	pushMatrix: () -> (),
	popMatrix: () -> (),
	resetMatrix: () -> (),
	translate: (x: number, y: number) -> (),
	translateScreen: (x: number, y: number) -> (),
	rotate: (angleDeg: number) -> (),
	scale: (sx: number, sy: number?) -> (),
}

declare input: {
	down: (key: string) -> boolean,
}

declare audio: {
	play: (name: string) -> (),
	thrust: (on: boolean) -> (),
}

declare app: {
	quit: () -> (),
}

-- Toolbar / viewport tooling (native-driven).
declare editor: {
	playState: () -> "stopped" | "playing" | "paused",
	getSelectedEntityId: () -> number,
}

-- Scene + script host API (game-facing facade).
declare runtime: {
	spawn: (name: string?) -> number,
	destroy: (id: number) -> (),
	clear: () -> (),
	loadScene: (path: string) -> (),
	addScript: (entityId: number, behaviorName: string, properties: { [string]: any }?) -> (),
	setScriptProperty: (entityId: number, scriptIndex: number, key: string, value: any?) -> (),
	setDrawOrder: (entityId: number, order: number) -> (),
	setUpdateOrder: (entityId: number, order: number) -> (),
	setName: (entityId: number, name: string) -> (),
	setActive: (entityId: number, active: boolean) -> (),
	getTransform: (entityId: number) -> TransformHandle,
	getWorldTransform: (entityId: number) -> WorldTransform,
	forEachEntityDrawOrder: ((entityId: number) -> ()) -> (),
	registerBehavior: (name: string, desc: BehaviorDesc) -> (),
	drawBehaviors: (totalTime: number) -> (),
}

export type BehaviorSelf = {
	entityId: number,
}

export type BehaviorDesc = {
	-- Optional schema table from defineProperties { ... } (see game/behavior_props).
	properties: any?,
	start: ((BehaviorSelf) -> ())?,
	update: ((BehaviorSelf, dt: number) -> ())?,
	draw: ((BehaviorSelf, totalTime: number) -> ())?,
	keydown: ((BehaviorSelf, key: string) -> ())?,
	onHudPlay: ((BehaviorSelf) -> ())?,
}

-- Set by the VM before behaviors load; behaviors may use without require.
declare defineProperties: (t: { [string]: any }) -> any
declare prop: {
	number: (default: number, meta: { min: number?, max: number?, slider: boolean? }?) -> any,
	integer: (default: number, meta: { min: number?, max: number?, slider: boolean? }?) -> any,
	string: (default: string) -> any,
	boolean: (default: boolean) -> any,
	enum: (default: string, options: { string }) -> any,
	object: (default: any?) -> any,
	asset: (default: string?) -> any,
	color: (r: number, g: number, b: number, a: number?) -> any,
	vector: (x: number, y: number) -> any,
}
