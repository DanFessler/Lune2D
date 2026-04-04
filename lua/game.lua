--!strict
-- Engine API types are declared in engine.d.luau (for luau-analyze).

local entities = require("entities")
local world = require("world")

-- ─── Types (local to this file) ───────────────────────────────────────────────

type Point        = { number }
type Polygon      = { Point }
type AsteroidSize = "large" | "medium" | "small"

type Asteroid = {
    id: string,
    name: string,
    draw: (Asteroid, number) -> (),
    x: number, y: number,
    vx: number, vy: number,
    angle: number,
    rotSpeed: number,
    size: AsteroidSize,
    r: number,
    shape: Polygon,
    dead: boolean?,
}

type GameState = "title" | "playing" | "gameover" | "newwave"

type Ship = {
    id: string,
    name: string,
    draw: (Ship, number) -> (),
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
    id: string,
    name: string,
    draw: (Bullet, number) -> (),
    x: number, y: number,
    angle: number,
    vx: number, vy: number,
    life: number,
    dead: boolean?,
}

--- Short-lived VFX: tiny asteroid shards (polygon) when a rock splits.
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

--- One hull line segment torn from the ship; spins and flies outward.
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

-- ─── Constants ────────────────────────────────────────────────────────────────

local PI: number = math.pi

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

local AST_SPLIT_DEBRIS_COUNT: { [AsteroidSize]: number } = { large = 16, medium = 12, small = 0 }

-- ─── Shapes ───────────────────────────────────────────────────────────────────

local SHIP_SHAPE:  Polygon = {{0, -SHIP_RADIUS*1.3}, {SHIP_RADIUS, SHIP_RADIUS}, {-SHIP_RADIUS, SHIP_RADIUS}}
local FLAME_SHAPE: Polygon = {{SHIP_RADIUS*0.6, SHIP_RADIUS}, {0, SHIP_RADIUS*2.1}, {-SHIP_RADIUS*0.6, SHIP_RADIUS}}
local MINI_SHIP:   Polygon = {{0, -9}, {7, 7}, {-7, 7}}

-- ─── Math helpers ─────────────────────────────────────────────────────────────

local function randf(lo: number, hi: number): number
    return lo + (hi - lo) * math.random()
end

local function wrap(x: number, y: number): (number, number)
    local W, H = screen.w, screen.h
    if x < 0 then x += W end
    if x > W then x -= W end
    if y < 0 then y += H end
    if y > H then y -= H end
    return x, y
end

-- Torus draw: 3×3 lattice (−W,0,+W × −H,0,+H) so entities can appear on both
-- sides of a wrap (up to all four corners) while logic stays in one period.
local function drawPolyWrapped(shape: Polygon, x: number, y: number, angle: number,
    r: number, g: number, b: number, a: number)
    local W, H = screen.w, screen.h
    for ox = -W, W, W do
        for oy = -H, H, H do
            draw.poly(shape, x + ox, y + oy, angle, r, g, b, a)
        end
    end
end

local function drawCircleWrapped(cx: number, cy: number, radius: number,
    r: number, g: number, b: number, a: number)
    local W, H = screen.w, screen.h
    for ox = -W, W, W do
        for oy = -H, H, H do
            draw.circle(cx + ox, cy + oy, radius, r, g, b, a)
        end
    end
end

local function drawLineWrapped(x1: number, y1: number, x2: number, y2: number,
    cr: number, cg: number, cb: number, ca: number)
    local W, H = screen.w, screen.h
    for ox = -W, W, W do
        for oy = -H, H, H do
            draw.line(x1 + ox, y1 + oy, x2 + ox, y2 + oy, cr, cg, cb, ca)
        end
    end
end

local function makeTinyShard(radius: number): Polygon
    local verts = 4 + math.random(0, 2)
    local shape: Polygon = {}
    for i = 1, verts do
        local ang = ((i - 1) / verts) * 2 * PI
        local ri = radius * randf(0.45, 1.0)
        shape[i] = { math.cos(ang) * ri, math.sin(ang) * ri }
    end
    return shape
end

local astDebris: { AstDebris } = {}
local shipDebris: { ShipDebris } = {}

local function spawnAsteroidSplitParticles(x: number, y: number, fromSize: AsteroidSize)
    local n = AST_SPLIT_DEBRIS_COUNT[fromSize]
    if n <= 0 then
        return
    end
    for _ = 1, n do
        local spd = randf(55, 170)
        local ang = randf(0, 2 * PI)
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
    local rad = shipAng * PI / 180
    local ca, sa = math.cos(rad), math.sin(rad)
    local verts = SHIP_SHAPE
    for e = 1, 3 do
        local p1, p2 = verts[e], verts[e % 3 + 1]
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
            local segAng = math.atan2(wdy, wdx) * 180 / PI
            local blast = randf(90, 220)
            local bang = randf(0, 2 * PI)
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
            p.x, p.y = wrap(p.x + p.vx * dt, p.y + p.vy * dt)
            liveA[#liveA + 1] = p
        end
    end
    astDebris = liveA

    local liveS: { ShipDebris } = {}
    for _, p in ipairs(shipDebris) do
        p.life -= dt
        if p.life > 0 then
            p.angle += p.rotSpeed * dt
            p.x, p.y = wrap(p.x + p.vx * dt, p.y + p.vy * dt)
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
        drawPolyWrapped(p.shape, p.x, p.y, p.angle, r, g, b, a)
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
        local lr = p.angle * PI / 180
        local c, s = math.cos(lr), math.sin(lr)
        local hx = p.halfLen
        local x1 = p.x - c * hx
        local y1 = p.y - s * hx
        local x2 = p.x + c * hx
        local y2 = p.y + s * hx
        drawLineWrapped(x1, y1, x2, y2, br, bg, bb, ba)
    end
end

-- Entity draw dispatch (transform is on self; custom per kind)
local function asteroid_draw(self: Asteroid, _t: number)
    drawPolyWrapped(self.shape, self.x, self.y, self.angle, 255, 255, 255, 255)
end

local function bullet_draw(self: Bullet, _t: number)
    drawCircleWrapped(self.x, self.y, BULLET_RADIUS, 0, 220, 255, 255)
end

local function ship_draw(self: Ship, totalTime: number)
    if not self.alive then
        return
    end
    local visible = self.respawnTimer <= 0 or (math.floor(totalTime * 8) % 2 == 0)
    if visible then
        local r: number, g: number, b: number
        if self.respawnTimer > 0 then
            r, g, b = 160, 160, 160
        else
            r, g, b = 255, 255, 255
        end
        drawPolyWrapped(SHIP_SHAPE, self.x, self.y, self.angle, r, g, b, 255)
        if self.thrusting and math.floor(totalTime * 20) % 2 == 0 then
            drawPolyWrapped(FLAME_SHAPE, self.x, self.y, self.angle, 255, 160, 0, 255)
        end
    end
end

local nextAstId: number = 0
local nextBulletId: number = 0

local function assignAstId(a: any)
    local ast = a :: Asteroid
    nextAstId += 1
    ast.id = "asteroid_" .. nextAstId
    ast.name = "Asteroid (" .. ast.size .. ")"
    ast.draw = asteroid_draw
end

local function assignBulletId(b: Bullet)
    nextBulletId += 1
    b.id = "bullet_" .. nextBulletId
    b.name = "Bullet " .. nextBulletId
    b.draw = bullet_draw
end

local function dist2(ax: number, ay: number, bx: number, by: number): number
    local dx, dy = ax - bx, ay - by
    return dx*dx + dy*dy
end

-- ─── Game state ───────────────────────────────────────────────────────────────

local ship: Ship = {
    id = "player",
    name = "Player",
    draw = ship_draw,
    x = screen.w / 2,
    y = screen.h / 2,
    vx = 0,
    vy = 0,
    angle = 270,
    thrusting = false,
    shootTimer = 0,
    respawnTimer = 0,
    alive = true,
    lives = 3,
}
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
    nextAstId = 0
    nextBulletId = 0
    ship = {
        id = "player",
        name = "Player",
        draw = ship_draw,
        x = screen.w / 2,
        y = screen.h / 2,
        vx = 0,
        vy = 0,
        angle = 270,
        thrusting = false,
        shootTimer = 0,
        respawnTimer = 0,
        alive = true,
        lives = 3,
    }
    bullets = {}
    asteroids = {}
    score = 0
    wave = 0
    state = "title"
    waveDelay = 1.5
    beatTimer = 0
    beatIndex = 0
    fps = 0
    fpsFrames = 0
    fpsAccum = 0
    astDebris = {}
    shipDebris = {}
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

    if state == "title" then
        world.rebuild(ship, asteroids, bullets)
        return
    end

    updateParticles(dt)

    if state ~= "gameover" then
        if state == "newwave" then
            waveDelay -= dt
            if waveDelay <= 0 then
                wave += 1
                asteroids = entities.spawnWave(2 + wave)
                for _, a in ipairs(asteroids) do
                    assignAstId(a)
                end
                state = "playing"
            end
        else
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
                id = "",
                name = "",
                draw = bullet_draw,
                x = tx,
                y = ty,
                angle = 0,
                vx = ship.vx + math.cos(rad) * BULLET_SPD,
                vy = ship.vy + math.sin(rad) * BULLET_SPD,
                life = BULLET_LIFE,
            }
            assignBulletId(b)
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
                            spawnAsteroidSplitParticles(a.x, a.y, a.size)
                            for _ = 1, 2 do
                                local spd = randf(40*1.3, 110*1.3)
                                local ang = randf(0, 2*PI)
                                local na = entities.makeAsteroid(
                                    a.x, a.y, nextSize,
                                    math.cos(ang) * spd, math.sin(ang) * spd)
                                assignAstId(na)
                                newAsts[#newAsts+1] = na
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
                spawnShipBreakup(ship.x, ship.y, ship.angle, ship.vx, ship.vy)
                ship.lives -= 1
                if ship.lives <= 0 then
                    ship.alive = false
                    state      = "gameover"
                else
                    ship.x, ship.y       = screen.w/2, screen.h/2
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
    end

    world.rebuild(ship, asteroids, bullets)
end

-- ─── Render ───────────────────────────────────────────────────────────────────

function _render(totalTime: number)
    local W, H = screen.w, screen.h
    draw.clear(0, 0, 0)

    world.draw_all(totalTime)

    drawParticles(totalTime)

    draw.number(score, 16, 16, 2.5, 255, 255, 255, 255)

    for i = 1, ship.lives do
        draw.poly(MINI_SHIP, W - 30 - (i-1)*28, 20, 270, 255, 255, 255, 255)
    end

    if state == "title" then
        local msg   = "READY"
        local scale = 4
        local cw    = 8 * scale
        local sx    = (W - #msg * cw) / 2
        for i = 1, #msg do
            draw.char(msg:sub(i,i), sx + (i-1)*cw, H/2 - 50, scale, 200, 220, 255, 255)
        end
        local pr  = "START IN LUA PANEL OR SPACE"
        local prx = (W - #pr * 8 * 1.25) / 2
        for i = 1, #pr do
            draw.char(pr:sub(i,i), prx + (i-1)*8*1.25, H/2 + 10, 1.25, 180, 180, 200, 255)
        end
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

function _on_hud_play()
    if state == "gameover" then
        resetGame()
    end
    if state == "title" then
        state     = "newwave"
        waveDelay = 1.5
    end
end

function _on_keydown(key: string)
    if key == "space" and state == "title" then
        _on_hud_play()
    elseif key == "r" and state == "gameover" then
        resetGame()
    elseif key == "escape" then
        app.quit()
    end
end
