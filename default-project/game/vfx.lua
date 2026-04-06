--!strict
-- Debris / particles (not scene entities).

local draw_util = require("game/draw_util")
local C = require("game/constants")

type Polygon = { { number } }
type Point = { number }

type AstDebris = {
	x: number,
	y: number,
	vx: number,
	vy: number,
	angle: number,
	rotSpeed: number,
	life: number,
	maxLife: number,
	shape: Polygon,
}

type ShipDebris = {
	x: number,
	y: number,
	vx: number,
	vy: number,
	angle: number,
	rotSpeed: number,
	halfLen: number,
	life: number,
	maxLife: number,
}

local function randf(lo: number, hi: number): number
	return lo + (hi - lo) * math.random()
end

local astDebris: { AstDebris } = {}
local shipDebris: { ShipDebris } = {}

local function makeTinyShard(radius: number): Polygon
	local verts = 4 + math.random(0, 2)
	local shape: Polygon = {}
	for i = 1, verts do
		local ang = ((i - 1) / verts) * 2 * C.PI
		local ri = radius * randf(0.45, 1.0)
		shape[i] = { math.cos(ang) * ri, math.sin(ang) * ri }
	end
	return shape
end

local function spawnAsteroidSplitParticles(x: number, y: number, fromSize: string)
	local n = (C.AST_SPLIT_DEBRIS_COUNT :: { [string]: number })[fromSize]
	if not n or n <= 0 then
		return
	end
	for _ = 1, n do
		local spd = randf(55, 170)
		local ang = randf(0, 2 * C.PI)
		local Rad = randf(1.8, 3.8)
		local life = randf(0.38, 0.72)
		astDebris[#astDebris + 1] = {
			x = x + randf(-2, 2),
			y = y + randf(-2, 2),
			vx = math.cos(ang) * spd,
			vy = math.sin(ang) * spd,
			angle = randf(0, 360),
			rotSpeed = randf(-420, 420),
			life = life,
			maxLife = life,
			shape = makeTinyShard(Rad),
		}
	end
end

local function spawnShipBreakup(sx: number, sy: number, shipAng: number, svx: number, svy: number)
	local rad = shipAng * C.PI / 180
	local ca, sa = math.cos(rad), math.sin(rad)
	local verts = C.SHIP_SHAPE
	for e = 1, 3 do
		local p1: Point = verts[e]
		local p2: Point = verts[e % 3 + 1]
		local mx = (p1[1] + p2[1]) * 0.5
		local my = (p1[2] + p2[2]) * 0.5
		local edx, edy = p2[1] - p1[1], p2[2] - p1[2]
		local elen = math.sqrt(edx * edx + edy * edy)
		if elen >= 1e-4 then
			local halfLen = elen * 0.5
			local wx = mx * ca - my * sa + sx
			local wy = mx * sa + my * ca + sy
			local wdx = edx * ca - edy * sa
			local wdy = edx * sa + edy * ca
			local segAng = math.atan2(wdy, wdx) * 180 / C.PI
			local blast = randf(90, 220)
			local bang = randf(0, 2 * C.PI)
			local life = randf(0.75, 1.25)
			shipDebris[#shipDebris + 1] = {
				x = wx,
				y = wy,
				vx = svx + math.cos(bang) * blast,
				vy = svy + math.sin(bang) * blast,
				angle = segAng,
				rotSpeed = randf(-540, 540),
				halfLen = halfLen,
				life = life,
				maxLife = life,
			}
		end
	end
end

local function updateParticles(dt: number)
	if dt <= 0 then
		return
	end
	local liveA: { AstDebris } = {}
	for _, p in ipairs(astDebris) do
		p.life -= dt
		if p.life > 0 then
			p.angle += p.rotSpeed * dt
			p.x, p.y = draw_util.wrap(p.x + p.vx * dt, p.y + p.vy * dt)
			liveA[#liveA + 1] = p
		end
	end
	astDebris = liveA

	local liveS: { ShipDebris } = {}
	for _, p in ipairs(shipDebris) do
		p.life -= dt
		if p.life > 0 then
			p.angle += p.rotSpeed * dt
			p.x, p.y = draw_util.wrap(p.x + p.vx * dt, p.y + p.vy * dt)
			liveS[#liveS + 1] = p
		end
	end
	shipDebris = liveS
end

local function drawParticles(_totalTime: number)
	for _, p in ipairs(astDebris) do
		local t = p.maxLife > 0 and (p.life / p.maxLife) or 0
		if t < 0 then
			t = 0
		elseif t > 1 then
			t = 1
		end
		local r = math.floor(220 * t + 0.5)
		local g = math.floor(220 * t + 0.5)
		local b = math.floor(230 * t + 0.5)
		local a = 255
		draw_util.drawPolyWrapped(p.shape, p.x, p.y, p.angle, r, g, b, a)
	end
	for _, p in ipairs(shipDebris) do
		local t = p.maxLife > 0 and (p.life / p.maxLife) or 0
		if t < 0 then
			t = 0
		elseif t > 1 then
			t = 1
		end
		local br = math.floor(255 * t + 0.5)
		local bg = math.floor(255 * t + 0.5)
		local bb = math.floor(255 * t + 0.5)
		local ba = 255
		local lr = p.angle * C.PI / 180
		local c, s = math.cos(lr), math.sin(lr)
		local hx = p.halfLen
		local x1 = p.x - c * hx
		local y1 = p.y - s * hx
		local x2 = p.x + c * hx
		local y2 = p.y + s * hx
		draw_util.drawLineWrapped(x1, y1, x2, y2, br, bg, bb, ba)
	end
end

local function clearVfx()
	astDebris = {}
	shipDebris = {}
end

return {
	spawnAsteroidSplitParticles = spawnAsteroidSplitParticles,
	spawnShipBreakup = spawnShipBreakup,
	updateParticles = updateParticles,
	drawParticles = drawParticles,
	clearVfx = clearVfx,
}
