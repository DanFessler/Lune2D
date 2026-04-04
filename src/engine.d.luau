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

-- Scene + script host API (game-facing facade).
declare runtime: {
	spawn: (name: string?) -> number,
	destroy: (id: number) -> (),
	clear: () -> (),
	loadScene: (path: string) -> (),
	addScript: (entityId: number, behaviorName: string) -> (),
	setDrawOrder: (entityId: number, order: number) -> (),
	setUpdateOrder: (entityId: number, order: number) -> (),
	setName: (entityId: number, name: string) -> (),
	setActive: (entityId: number, active: boolean) -> (),
	getTransform: (entityId: number) -> TransformHandle,
	getWorldTransform: (entityId: number) -> WorldTransform,
	registerBehavior: (name: string, desc: BehaviorDesc) -> (),
	drawBehaviors: (totalTime: number) -> (),
}

export type BehaviorDesc = {
	start: ((entityId: number) -> ())?,
	update: ((entityId: number, dt: number) -> ())?,
	draw: ((entityId: number, totalTime: number) -> ())?,
	keydown: ((entityId: number, key: string) -> ())?,
	onHudPlay: ((entityId: number) -> ())?,
}
