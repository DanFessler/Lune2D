--!strict
-- Shared numeric constants and polygon shapes for the asteroids game.

local SHIP_RADIUS: number = 14
local sr = SHIP_RADIUS

local M = {
	PI = math.pi,
	SHIP_ROT_SPD = 200,
	SHIP_THRUST = 250,
	SHIP_MAX_SPD = 400,
	SHIP_RADIUS = SHIP_RADIUS,
	BULLET_SPD = 520,
	BULLET_LIFE = 1.1,
	BULLET_RADIUS = 3,
	SHOOT_COOLDOWN = 0.01,
	-- After losing a life: hidden wait, then blink-invuln at center (seconds).
	SHIP_DEATH_RESPAWN_DELAY = 1.5,
	SHIP_POST_RESPAWN_INVULN = 2.5,
	AST_SCORE = { large = 20, medium = 50, small = 100 },
	AST_SOUND = { large = "exp_large", medium = "exp_medium", small = "exp_small" },
	AST_SPLIT = { large = "medium", medium = "small", small = nil :: any },
	AST_SPLIT_DEBRIS_COUNT = { large = 16, medium = 12, small = 0 },
	SHIP_SHAPE = { { 0, -sr * 1.3 }, { sr, sr }, { -sr, sr } },
	FLAME_SHAPE = { { sr * 0.6, sr }, { 0, sr * 2.1 }, { -sr * 0.6, sr } },
	MINI_SHIP = { { 0, -9 }, { 7, 7 }, { -7, 7 } },
}

return M
