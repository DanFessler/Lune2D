--!strict
-- Entity types, constants, and factory functions for asteroids and waves.

export type Point        = { number }
export type Polygon      = { Point }
export type AsteroidSize = "large" | "medium" | "small"

export type Asteroid = {
    x: number, y: number,
    vx: number, vy: number,
    angle: number,
    rotSpeed: number,
    size: AsteroidSize,
    r: number,
    shape: Polygon,
    dead: boolean?,
}

-- ─── Constants ────────────────────────────────────────────────────────────────

local PI: number = math.pi

local AST_LARGE_R:  number = 42
local AST_MEDIUM_R: number = 24
local AST_SMALL_R:  number = 13
local AST_MIN_SPD:  number = 40
local AST_MAX_SPD:  number = 110

local AST_RADIUS: { [AsteroidSize]: number } = {
    large = AST_LARGE_R, medium = AST_MEDIUM_R, small = AST_SMALL_R,
}

-- ─── Helpers ──────────────────────────────────────────────────────────────────

local function randf(lo: number, hi: number): number
    return lo + (hi - lo) * math.random()
end

-- ─── Factory functions ────────────────────────────────────────────────────────

local function makeAsteroid(x: number, y: number, size: AsteroidSize, vx: number?, vy: number?): Asteroid
    local r = AST_RADIUS[size]
    local avx: number, avy: number
    if vx and vy then
        avx, avy = vx, vy
    else
        local spd = randf(AST_MIN_SPD, AST_MAX_SPD)
        local ang = randf(0, 2*PI)
        avx, avy = math.cos(ang)*spd, math.sin(ang)*spd
    end
    local verts = 10 + math.random(0, 3)
    local shape: Polygon = {}
    for i = 1, verts do
        local ang = ((i-1) / verts) * 2*PI
        local ri  = r * randf(0.6, 1.0)
        shape[i]  = {math.cos(ang)*ri, math.sin(ang)*ri}
    end
    return {
        x=x, y=y, vx=avx, vy=avy,
        angle    = randf(0, 360),
        rotSpeed = randf(20, 90) * (math.random(2) == 1 and 1 or -1),
        size = size, r = r, shape = shape,
    }
end

local function spawnWave(count: number): { Asteroid }
    local wave: { Asteroid } = {}
    for _ = 1, count do
        local edge = math.random(4)
        local x: number, y: number
        local W, H = screen.w, screen.h
        local hw, hh = W / 2, H / 2
        if edge == 1 then
            x = randf(-hw, hw)
            y = -hh - AST_LARGE_R
        elseif edge == 2 then
            x = randf(-hw, hw)
            y = hh + AST_LARGE_R
        elseif edge == 3 then
            x = -hw - AST_LARGE_R
            y = randf(-hh, hh)
        else
            x = hw + AST_LARGE_R
            y = randf(-hh, hh)
        end
        wave[#wave+1] = makeAsteroid(x, y, "large")
    end
    return wave
end

-- ─── Module exports ───────────────────────────────────────────────────────────

return {
    makeAsteroid = makeAsteroid,
    spawnWave    = spawnWave,
}
