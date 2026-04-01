-- game.lua — all gameplay logic for Asteroids

local PI  = math.pi
local W   = screen.w
local H   = screen.h

-- ─── Constants ────────────────────────────────────────────────────────────────

local SHIP_ROT_SPD   = 200
local SHIP_THRUST    = 250
local SHIP_MAX_SPD   = 400
local SHIP_RADIUS    = 14
local BULLET_SPD     = 520
local BULLET_LIFE    = 1.1
local BULLET_RADIUS  = 3
local SHOOT_COOLDOWN = 0.18
local AST_LARGE_R    = 42
local AST_MEDIUM_R   = 24
local AST_SMALL_R    = 13
local AST_MIN_SPD    = 40
local AST_MAX_SPD    = 110

local AST_RADIUS = { large = AST_LARGE_R, medium = AST_MEDIUM_R, small = AST_SMALL_R }
local AST_SCORE  = { large = 20,          medium = 50,           small = 100 }
local AST_SOUND  = { large = "exp_large", medium = "exp_medium", small = "exp_small" }
local AST_SPLIT  = { large = "medium",    medium = "small" }

-- ─── Shapes ───────────────────────────────────────────────────────────────────

local SHIP_SHAPE  = {{0, -SHIP_RADIUS*1.3}, {SHIP_RADIUS, SHIP_RADIUS}, {-SHIP_RADIUS, SHIP_RADIUS}}
local FLAME_SHAPE = {{SHIP_RADIUS*0.6, SHIP_RADIUS}, {0, SHIP_RADIUS*2.1}, {-SHIP_RADIUS*0.6, SHIP_RADIUS}}
local MINI_SHIP   = {{0,-9},{7,7},{-7,7}}

-- ─── Math helpers ─────────────────────────────────────────────────────────────

local function randf(lo, hi) return lo + (hi - lo) * math.random() end

local function wrap(x, y)
    if x < 0   then x = x + W end
    if x > W   then x = x - W end
    if y < 0   then y = y + H end
    if y > H   then y = y - H end
    return x, y
end

local function dist2(ax, ay, bx, by)
    local dx, dy = ax - bx, ay - by
    return dx*dx + dy*dy
end

-- ─── Asteroid factory ─────────────────────────────────────────────────────────

local function makeAsteroid(x, y, size, vx, vy)
    local r = AST_RADIUS[size]
    if not vx then
        local spd = randf(AST_MIN_SPD, AST_MAX_SPD)
        local ang = randf(0, 2*PI)
        vx, vy = math.cos(ang)*spd, math.sin(ang)*spd
    end
    local verts = 10 + math.random(0, 3)
    local shape = {}
    for i = 1, verts do
        local ang = ((i-1) / verts) * 2*PI
        local ri  = r * randf(0.6, 1.0)
        shape[i]  = {math.cos(ang)*ri, math.sin(ang)*ri}
    end
    return { x=x, y=y, vx=vx, vy=vy, angle=randf(0,360),
             rotSpeed=randf(20,90) * (math.random(2)==1 and 1 or -1),
             size=size, r=r, shape=shape }
end

local function spawnWave(count)
    local asteroids = {}
    for _ = 1, count do
        local edge = math.random(4)
        local x, y
        if     edge == 1 then x = randf(0,W); y = -AST_LARGE_R
        elseif edge == 2 then x = randf(0,W); y = H + AST_LARGE_R
        elseif edge == 3 then x = -AST_LARGE_R; y = randf(0,H)
        else                   x = W + AST_LARGE_R; y = randf(0,H) end
        asteroids[#asteroids+1] = makeAsteroid(x, y, "large")
    end
    return asteroids
end

-- ─── Game state ───────────────────────────────────────────────────────────────

local ship, bullets, asteroids
local score, wave
local state, waveDelay
local beatTimer, beatIndex
local fps, fpsFrames, fpsAccum

local function resetGame()
    ship = {
        x=W/2, y=H/2, vx=0, vy=0,
        angle=270, thrusting=false,
        shootTimer=0, respawnTimer=0,
        alive=true, lives=3
    }
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

function game_init()
    math.randomseed(os.time())
    resetGame()
end

-- ─── Update ───────────────────────────────────────────────────────────────────

function game_update(dt, totalTime)
    -- FPS counter
    fpsAccum  = fpsAccum + dt
    fpsFrames = fpsFrames + 1
    if fpsAccum >= 0.5 then
        fps       = math.floor(fpsFrames / fpsAccum + 0.5)
        fpsFrames = 0
        fpsAccum  = 0
    end

    if state == "gameover" then return end

    -- Wave transition
    if state == "newwave" then
        waveDelay = waveDelay - dt
        if waveDelay <= 0 then
            wave      = wave + 1
            asteroids = spawnWave(2 + wave)
            state     = "playing"
        end
        return
    end

    -- ── Ship ──────────────────────────────────────────────────────────────
    local s = ship
    s.shootTimer = s.shootTimer - dt
    if s.respawnTimer > 0 then s.respawnTimer = s.respawnTimer - dt end

    if s.alive then
        if input.down("left") or input.down("a") then
            s.angle = s.angle - SHIP_ROT_SPD * dt
        end
        if input.down("right") or input.down("d") then
            s.angle = s.angle + SHIP_ROT_SPD * dt
        end

        local thrusting = input.down("up") or input.down("w")
        s.thrusting = thrusting
        if thrusting then
            local rad = (s.angle - 90) * PI / 180
            s.vx = s.vx + math.cos(rad) * SHIP_THRUST * dt
            s.vy = s.vy + math.sin(rad) * SHIP_THRUST * dt
        end
        audio.thrust(thrusting)

        local spd2 = s.vx*s.vx + s.vy*s.vy
        if spd2 > SHIP_MAX_SPD*SHIP_MAX_SPD then
            local sc = SHIP_MAX_SPD / math.sqrt(spd2)
            s.vx, s.vy = s.vx*sc, s.vy*sc
        end

        s.x, s.y = wrap(s.x + s.vx*dt, s.y + s.vy*dt)

        -- Shoot
        if input.down("space") and s.shootTimer <= 0 then
            s.shootTimer = SHOOT_COOLDOWN
            local rad = (s.angle - 90) * PI / 180
            local tx  = s.x + math.cos(rad) * SHIP_RADIUS * 1.3
            local ty  = s.y + math.sin(rad) * SHIP_RADIUS * 1.3
            bullets[#bullets+1] = {
                x=tx, y=ty,
                vx=s.vx + math.cos(rad)*BULLET_SPD,
                vy=s.vy + math.sin(rad)*BULLET_SPD,
                life=BULLET_LIFE,
            }
            audio.play("shoot")
        end
    end

    -- ── Bullets ───────────────────────────────────────────────────────────
    local aliveB = {}
    for _, b in ipairs(bullets) do
        b.life = b.life - dt
        if b.life > 0 then
            b.x, b.y = wrap(b.x + b.vx*dt, b.y + b.vy*dt)
            aliveB[#aliveB+1] = b
        end
    end
    bullets = aliveB

    -- ── Asteroids ─────────────────────────────────────────────────────────
    for _, a in ipairs(asteroids) do
        a.x, a.y = wrap(a.x + a.vx*dt, a.y + a.vy*dt)
        a.angle  = a.angle + a.rotSpeed * dt
    end

    -- ── Bullet × Asteroid collision ───────────────────────────────────────
    local newAsts = {}
    for _, b in ipairs(bullets) do
        if not b.dead then
            for _, a in ipairs(asteroids) do
                if not a.dead then
                    local r = a.r + BULLET_RADIUS
                    if dist2(b.x, b.y, a.x, a.y) < r*r then
                        b.dead = true
                        a.dead = true
                        score  = score + AST_SCORE[a.size]
                        audio.play(AST_SOUND[a.size])
                        local nextSize = AST_SPLIT[a.size]
                        if nextSize then
                            for _ = 1, 2 do
                                local spd = randf(AST_MIN_SPD*1.3, AST_MAX_SPD*1.3)
                                local ang = randf(0, 2*PI)
                                newAsts[#newAsts+1] = makeAsteroid(
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

    -- Filter dead objects and append new asteroids
    local liveB = {}
    for _, b in ipairs(bullets) do
        if not b.dead then liveB[#liveB+1] = b end
    end
    bullets = liveB

    local liveA = {}
    for _, a in ipairs(asteroids) do
        if not a.dead then liveA[#liveA+1] = a end
    end
    for _, na in ipairs(newAsts) do liveA[#liveA+1] = na end
    asteroids = liveA

    -- ── Ship × Asteroid collision ─────────────────────────────────────────
    if s.alive and s.respawnTimer <= 0 then
        for _, a in ipairs(asteroids) do
            local r = a.r + SHIP_RADIUS
            if dist2(s.x, s.y, a.x, a.y) < r*r then
                audio.play("death")
                s.lives = s.lives - 1
                if s.lives <= 0 then
                    s.alive = false
                    state   = "gameover"
                else
                    s.x, s.y       = W/2, H/2
                    s.vx, s.vy     = 0, 0
                    s.angle        = 270
                    s.respawnTimer = 2.5
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
        beatTimer = beatTimer - dt
        if beatTimer <= 0 then
            audio.play("beat" .. beatIndex)
            beatIndex = 1 - beatIndex
            beatTimer = interval
        end
    end
end

-- ─── Render ───────────────────────────────────────────────────────────────────

function game_render(totalTime)
    draw.clear(0, 0, 0)

    -- Asteroids
    for _, a in ipairs(asteroids) do
        draw.poly(a.shape, a.x, a.y, a.angle, 255, 255, 255, 255)
    end

    -- Bullets
    for _, b in ipairs(bullets) do
        draw.circle(b.x, b.y, BULLET_RADIUS, 0, 220, 255, 255)
    end

    -- Ship
    local s = ship
    if s.alive then
        local visible = s.respawnTimer <= 0 or (math.floor(totalTime * 8) % 2 == 0)
        if visible then
            local r, g, b = 255, 255, 255
            if s.respawnTimer > 0 then r, g, b = 160, 160, 160 end
            draw.poly(SHIP_SHAPE, s.x, s.y, s.angle, r, g, b, 255)
            if s.thrusting and math.floor(totalTime * 20) % 2 == 0 then
                draw.poly(FLAME_SHAPE, s.x, s.y, s.angle, 255, 160, 0, 255)
            end
        end
    end

    -- Score
    draw.number(score, 16, 16, 2.5, 255, 255, 255, 255)

    -- Lives
    for i = 1, s.lives do
        draw.poly(MINI_SHIP, W - 30 - (i-1)*28, 20, 270, 255, 255, 255, 255)
    end

    -- Game over overlay
    if state == "gameover" then
        local msg = "GAME OVER"
        local scale, cw = 4, 8*4
        local sx = (W - #msg * cw) / 2
        for i = 1, #msg do
            draw.char(msg:sub(i,i), sx + (i-1)*cw, H/2 - 40, scale, 255, 80, 80, 255)
        end
        local sc = "SCORE"
        local scx = (W - #sc * 8 * 2.5) / 2 - 20
        for i = 1, #sc do
            draw.char(sc:sub(i,i), scx + (i-1)*8*2.5, H/2 + 20, 2.5, 255, 255, 255, 255)
        end
        draw.number(score, scx + #sc*8*2.5 + 12, H/2 + 20, 2.5, 0, 220, 255, 255)
        local pr = "PRESS R TO RESTART"
        local prx = (W - #pr * 8 * 1.5) / 2
        for i = 1, #pr do
            draw.char(pr:sub(i,i), prx + (i-1)*8*1.5, H/2 + 70, 1.5, 160, 160, 160, 255)
        end
    end

    -- Wave announcement
    if state == "newwave" and wave > 0 then
        local wt = "WAVE"
        local wx = (W - #wt * 8 * 3) / 2 - 30
        for i = 1, #wt do
            draw.char(wt:sub(i,i), wx + (i-1)*8*3, H/2 - 30, 3, 100, 255, 140, 255)
        end
        draw.number(wave, wx + #wt*8*3 + 8, H/2 - 30, 3, 100, 255, 140, 255)
    end

    -- FPS
    draw.number(fps, 16, H - 22, 1.2, 80, 80, 80, 255)

    draw.present()
end

-- ─── Key events ───────────────────────────────────────────────────────────────

function game_on_keydown(key)
    if key == "r" and state == "gameover" then
        resetGame()
    elseif key == "escape" then
        app.quit()
    end
end
