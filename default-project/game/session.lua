--!strict
-- Mutable session shared by behaviors and game.lua (particles, HUD).

export type GameState = "title" | "playing" | "gameover" | "newwave"
export type AsteroidSize = "large" | "medium" | "small"
export type Polygon = { { number } }

export type AsteroidExtra = {
	size: AsteroidSize,
	shape: Polygon,
	r: number,
	rotSpeed: number,
	dead: boolean?,
}

export type BulletExtra = {
	life: number,
	dead: boolean?,
}

export type ShipExtra = {
	thrusting: boolean,
	shootTimer: number,
	--- Counts down after death (lives left); ship hidden until 0, then warp + invuln.
	respawnDelayTimer: number,
	--- Blink invulnerability after respawn at center.
	respawnTimer: number,
	alive: boolean,
	lives: number,
}

local session = {
	state = "title" :: GameState,
	score = 0,
	wave = 0,
	waveDelay = 0,
	beatTimer = 0,
	beatIndex = 0,
	shipEntityId = nil :: number?,
	directorEntityId = nil :: number?,
	asteroids = {} :: { [number]: AsteroidExtra },
	bullets = {} :: { [number]: BulletExtra },
	ship = {
		thrusting = false,
		shootTimer = 0,
		respawnDelayTimer = 0,
		respawnTimer = 0,
		alive = true,
		lives = 3,
	} :: ShipExtra,
}

return session
