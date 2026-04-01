--!strict
-- Engine API types are declared in engine.d.luau (for luau-analyze).

local entities = require("entities")

-- ─── Types (local to this file) ───────────────────────────────────────────────

type Point        = { number }
type Polygon      = { Point }
type AsteroidSize = "large" | "medium" | "small"

type Asteroid = {
    x: number, y: number,
    vx: number, vy: number,
    angle: number,
    rotSpeed: number,
    size: AsteroidSize,
    r: number,
    shape: Polygon,
    dead: boolean?,
}

type GameState = "playing" | "gameover" | "newwave"

type Ship = {
    x: number, y: number,
    vx: number, vy: number,
    angle: number,
    thrusting: boolean,
    shootTimer: number,
    respawnTimer: number,
    alive: boolean,
    lives: number,
}

type Bullet = {
    x: number, y: number,
    vx: number, vy: number,
    life: number,
    dead: boolean?,
}

-- ─── Constants ────────────────────────────────────────────────────────────────

local PI: number = math.pi
local W: number  = screen.w
local H: number  = screen.h

local SHIP_ROT_SPD:   number = 200
local SHIP_THRUST:    number = 250
local SHIP_MAX_SPD:   number = 400
local SHIP_RADIUS:    number = 14
local BULLET_SPD:     number = 520
local BULLET_LIFE:    number = 1.1
local BULLET_RADIUS:  number = 3
local SHOOT_COOLDOWN: number = 0.18

local AST_SCORE: { [AsteroidSize]: number }        = { large = 20,          medium = 50,           small = 100 }
local AST_SOUND: { [AsteroidSize]: string }        = { large = "exp_large", medium = "exp_medium", small = "exp_small" }
local AST_SPLIT: { [AsteroidSize]: AsteroidSize? } = { large = "medium",    medium = "small",      small = nil }

-- ─── Shapes ───────────────────────────────────────────────────────────────────

local SHIP_SHAPE:  Polygon = {{0, -SHIP_RADIUS*1.3}, {SHIP_RADIUS, SHIP_RADIUS}, {-SHIP_RADIUS, SHIP_RADIUS}}
local FLAME_SHAPE: Polygon = {{SHIP_RADIUS*0.6, SHIP_RADIUS}, {0, SHIP_RADIUS*2.1}, {-SHIP_RADIUS*0.6, SHIP_RADIUS}}
local MINI_SHIP:   Polygon = {{0, -9}, {7, 7}, {-7, 7}}

-- ─── Math helpers ─────────────────────────────────────────────────────────────

local function randf(lo: number, hi: number): number
    return lo + (hi - lo) * math.random()
end

local function wrap(x: number, y: number): (number, number)
    if x < 0 then x += W end
    if x > W then x -= W end
    if y < 0 then y += H end
    if y > H then y -= H end
    return x, y
end

local function dist2(ax: number, ay: number, bx: number, by: number): number
    local dx, dy = ax - bx, ay - by
    return dx*dx + dy*dy
end

-- ─── Game state ───────────────────────────────────────────────────────────────

local ship: Ship = { x=W/2, y=H/2, vx=0, vy=0, angle=270, thrusting=false,
                     shootTimer=0, respawnTimer=0, alive=true, lives=3 }
local bullets: { Bullet }     = {}
local asteroids: { Asteroid } = {}
local score: number     = 0
local wave: number      = 0
local state: GameState  = "newwave"
local waveDelay: number = 1.5
local beatTimer: number = 0
local beatIndex: number = 0
local fps: number       = 0
local fpsFrames: number = 0
local fpsAccum: number  = 0

local function resetGame()
    ship       = { x=W/2, y=H/2, vx=0, vy=0, angle=270, thrusting=false,
                   shootTimer=0, respawnTimer=0, alive=true, lives=3 }
    bullets    = {}
    asteroids  = {}
    score      = 0
    wave       = 0
    state      = "newwave"
    waveDelay  = 1.5
    beatTimer  = 0
    beatIndex  = 0
    fps        = 0
    fpsFrames  = 0
    fpsAccum   = 0
end

-- ─── Init ─────────────────────────────────────────────────────────────────────

function _init()
    math.randomseed(os.time())
    resetGame()
end

-- ─── Update ───────────────────────────────────────────────────────────────────

function _update(dt: number, _totalTime: number)
    fpsAccum  += dt
    fpsFrames += 1
    if fpsAccum >= 0.5 then
        fps       = math.floor(fpsFrames / fpsAccum + 0.5)
        fpsFrames = 0
        fpsAccum  = 0
    end

    if state == "gameover" then return end

    if state == "newwave" then
        waveDelay -= dt
        if waveDelay <= 0 then
            wave      += 1
            asteroids  = entities.spawnWave(2 + wave)
            state      = "playing"
        end
        return
    end

    -- ── Ship ──────────────────────────────────────────────────────────────
    ship.shootTimer -= dt
    if ship.respawnTimer > 0 then ship.respawnTimer -= dt end

    if ship.alive then
        if input.down("left") or input.down("a") then
            ship.angle -= SHIP_ROT_SPD * dt
        end
        if input.down("right") or input.down("d") then
            ship.angle += SHIP_ROT_SPD * dt
        end

        local thrusting = input.down("up") or input.down("w")
        ship.thrusting  = thrusting
        if thrusting then
            local rad = (ship.angle - 90) * PI / 180
            ship.vx  += math.cos(rad) * SHIP_THRUST * dt
            ship.vy  += math.sin(rad) * SHIP_THRUST * dt
        end
        audio.thrust(thrusting)

        local spd2 = ship.vx*ship.vx + ship.vy*ship.vy
        if spd2 > SHIP_MAX_SPD*SHIP_MAX_SPD then
            local sc = SHIP_MAX_SPD / math.sqrt(spd2)
            ship.vx *= sc
            ship.vy *= sc
        end

        ship.x, ship.y = wrap(ship.x + ship.vx*dt, ship.y + ship.vy*dt)

        if input.down("space") and ship.shootTimer <= 0 then
            ship.shootTimer = SHOOT_COOLDOWN
            local rad = (ship.angle - 90) * PI / 180
            local tx  = ship.x + math.cos(rad) * SHIP_RADIUS * 1.3
            local ty  = ship.y + math.sin(rad) * SHIP_RADIUS * 1.3
            local b: Bullet = {
                x  = tx, y  = ty,
                vx = ship.vx + math.cos(rad)*BULLET_SPD,
                vy = ship.vy + math.sin(rad)*BULLET_SPD,
                life = BULLET_LIFE,
            }
            bullets[#bullets+1] = b
            audio.play("shoot")
        end
    end

    -- ── Bullets ───────────────────────────────────────────────────────────
    local aliveB: { Bullet } = {}
    for _, b in ipairs(bullets) do
        b.life -= dt
        if b.life > 0 then
            b.x, b.y = wrap(b.x + b.vx*dt, b.y + b.vy*dt)
            aliveB[#aliveB+1] = b
        end
    end
    bullets = aliveB

    -- ── Asteroids ─────────────────────────────────────────────────────────
    for _, a in ipairs(asteroids) do
        a.x, a.y = wrap(a.x + a.vx*dt, a.y + a.vy*dt)
        a.angle += a.rotSpeed * dt
    end

    -- ── Bullet × Asteroid collision ───────────────────────────────────────
    local newAsts: { Asteroid } = {}
    for _, b in ipairs(bullets) do
        if not b.dead then
            for _, a in ipairs(asteroids) do
                if not a.dead then
                    local r = a.r + BULLET_RADIUS
                    if dist2(b.x, b.y, a.x, a.y) < r*r then
                        b.dead = true
                        a.dead = true
                        score += AST_SCORE[a.size]
                        audio.play(AST_SOUND[a.size])
                        local nextSize: AsteroidSize? = AST_SPLIT[a.size]
                        if nextSize then
                            for _ = 1, 2 do
                                local spd = randf(40*1.3, 110*1.3)
                                local ang = randf(0, 2*PI)
                                newAsts[#newAsts+1] = entities.makeAsteroid(
                                    a.x, a.y, nextSize,
                                    math.cos(ang)*spd, math.sin(ang)*spd)
                            end
                        end
                        break
                    end
                end
            end
        end
    end

    local liveB: { Bullet } = {}
    for _, b in ipairs(bullets) do
        if not b.dead then liveB[#liveB+1] = b end
    end
    bullets = liveB

    local liveA: { Asteroid } = {}
    for _, a in ipairs(asteroids) do
        if not a.dead then liveA[#liveA+1] = a end
    end
    for _, na in ipairs(newAsts) do liveA[#liveA+1] = na end
    asteroids = liveA

    -- ── Ship × Asteroid collision ─────────────────────────────────────────
    if ship.alive and ship.respawnTimer <= 0 then
        for _, a in ipairs(asteroids) do
            local r = a.r + SHIP_RADIUS
            if dist2(ship.x, ship.y, a.x, a.y) < r*r then
                audio.play("death")
                ship.lives -= 1
                if ship.lives <= 0 then
                    ship.alive = false
                    state      = "gameover"
                else
                    ship.x, ship.y       = W/2, H/2
                    ship.vx, ship.vy     = 0, 0
                    ship.angle           = 270
                    ship.respawnTimer    = 2.5
                end
                break
            end
        end
    end

    -- ── Wave clear ────────────────────────────────────────────────────────
    if #asteroids == 0 and state == "playing" then
        state     = "newwave"
        waveDelay = 2.0
    end

    -- ── Heartbeat ─────────────────────────────────────────────────────────
    if #asteroids > 0 then
        local maxAsts  = (2 + wave) * 4
        local ratio    = math.min(#asteroids / maxAsts, 1)
        local interval = 0.22 + ratio * 0.58
        beatTimer -= dt
        if beatTimer <= 0 then
            audio.play("beat" .. beatIndex)
            beatIndex = 1 - beatIndex
            beatTimer = interval
        end
    end
end

-- ─── Render ───────────────────────────────────────────────────────────────────

function _render(totalTime: number)
    draw.clear(0, 0, 0)

    for _, a in ipairs(asteroids) do
        draw.poly(a.shape, a.x, a.y, a.angle, 255, 255, 255, 255)
    end

    for _, b in ipairs(bullets) do
        draw.circle(b.x, b.y, BULLET_RADIUS, 0, 220, 255, 255)
    end

    if ship.alive then
        local visible = ship.respawnTimer <= 0 or (math.floor(totalTime * 8) % 2 == 0)
        if visible then
            local r: number, g: number, b: number
            if ship.respawnTimer > 0 then r, g, b = 160, 160, 160 else r, g, b = 255, 255, 255 end
            draw.poly(SHIP_SHAPE, ship.x, ship.y, ship.angle, r, g, b, 255)
            if ship.thrusting and math.floor(totalTime * 20) % 2 == 0 then
                draw.poly(FLAME_SHAPE, ship.x, ship.y, ship.angle, 255, 160, 0, 255)
            end
        end
    end

    draw.number(score, 16, 16, 2.5, 255, 255, 255, 255)

    for i = 1, ship.lives do
        draw.poly(MINI_SHIP, W - 30 - (i-1)*28, 20, 270, 255, 255, 255, 255)
    end

    if state == "gameover" then
        local msg   = "GAME OVER"
        local scale = 4
        local cw    = 8 * scale
        local sx    = (W - #msg * cw) / 2
        for i = 1, #msg do
            draw.char(msg:sub(i,i), sx + (i-1)*cw, H/2 - 40, scale, 255, 80, 80, 255)
        end
        local sc  = "SCORE"
        local scx = (W - #sc * 8 * 2.5) / 2 - 20
        for i = 1, #sc do
            draw.char(sc:sub(i,i), scx + (i-1)*8*2.5, H/2 + 20, 2.5, 255, 255, 255, 255)
        end
        draw.number(score, scx + #sc*8*2.5 + 12, H/2 + 20, 2.5, 0, 220, 255, 255)
        local pr  = "PRESS R TO RESTART"
        local prx = (W - #pr * 8 * 1.5) / 2
        for i = 1, #pr do
            draw.char(pr:sub(i,i), prx + (i-1)*8*1.5, H/2 + 70, 1.5, 160, 160, 160, 255)
        end
    end

    if state == "newwave" and wave > 0 then
        local wt  = "WAVE"
        local wtx = (W - #wt * 8 * 3) / 2 - 30
        for i = 1, #wt do
            draw.char(wt:sub(i,i), wtx + (i-1)*8*3, H/2 - 30, 3, 100, 255, 140, 255)
        end
        draw.number(wave, wtx + #wt*8*3 + 8, H/2 - 30, 3, 100, 255, 140, 255)
    end

    draw.number(fps, 16, H - 22, 1.2, 80, 80, 80, 255)

    draw.present()
end

-- ─── Key events ───────────────────────────────────────────────────────────────

function _on_keydown(key: string)
    if key == "r" and state == "gameover" then
        resetGame()
    elseif key == "escape" then
        app.quit()
    end
end
