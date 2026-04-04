--!strict
local session = require("game/session")
local C = require("game/constants")
local vfx = require("game/vfx")
local entities = require("entities")

type AsteroidSize = "large" | "medium" | "small"

local function randf(lo: number, hi: number): number
	return lo + (hi - lo) * math.random()
end

local function dist2(ax: number, ay: number, bx: number, by: number): number
	local dx, dy = ax - bx, ay - by
	return dx * dx + dy * dy
end

local function destroyAsteroid(id: number)
	session.asteroids[id] = nil
	runtime.destroy(id)
end

local function destroyBullet(id: number)
	session.bullets[id] = nil
	runtime.destroy(id)
end

return {
	start = function(id: number)
		session.state = "title"
		session.score = 0
		session.wave = 0
		session.waveDelay = 0
		session.beatTimer = 0
		session.beatIndex = 0
		session.directorEntityId = id
		session.asteroids = {}
		session.bullets = {}
	end,
	update = function(_entityId: number, dt: number)
		local st = session.state
		if st == "title" or st == "gameover" then
			return
		end

		if st == "newwave" then
			session.waveDelay -= dt
			if session.waveDelay <= 0 then
				session.wave += 1
				local waveAsts = entities.spawnWave(2 + session.wave)
				for _, a in ipairs(waveAsts) do
					local aid = runtime.spawn("Asteroid")
					runtime.setDrawOrder(aid, 0)
					runtime.setUpdateOrder(aid, 2)
					local t = runtime.getTransform(aid)
					t.x = a.x
					t.y = a.y
					t.angle = a.angle
					t.vx = a.vx
					t.vy = a.vy
					session.asteroids[aid] = {
						size = a.size,
						shape = a.shape,
						r = a.r,
						rotSpeed = a.rotSpeed,
					}
					runtime.addScript(aid, "Asteroid")
				end
				session.state = "playing"
			end
			-- Stay in this frame: ship, bullets, and leftovers keep simulating during inter-wave delay.
		end

		-- playing (also runs while `newwave` is counting down between waves)
		local shipId = session.shipEntityId
		local shipEnt = session.ship

		-- bullet vs asteroid
		for bid, b in pairs(session.bullets) do
			if not b.dead then
				local bt = runtime.getTransform(bid)
				for aid, a in pairs(session.asteroids) do
					if not a.dead then
						local at = runtime.getTransform(aid)
						local r = a.r + C.BULLET_RADIUS
						if dist2(bt.x, bt.y, at.x, at.y) < r * r then
							b.dead = true
							a.dead = true
							session.score += C.AST_SCORE[a.size]
							audio.play(C.AST_SOUND[a.size])
							local nextSize: AsteroidSize? = (C.AST_SPLIT :: { [string]: AsteroidSize? })[a.size]
							if nextSize then
								vfx.spawnAsteroidSplitParticles(at.x, at.y, a.size)
								for _ = 1, 2 do
									local spd = randf(40 * 1.3, 110 * 1.3)
									local ang = randf(0, 2 * C.PI)
									local na = entities.makeAsteroid(
										at.x,
										at.y,
										nextSize,
										math.cos(ang) * spd,
										math.sin(ang) * spd
									)
									local nid = runtime.spawn("Asteroid")
									runtime.setDrawOrder(nid, 0)
									runtime.setUpdateOrder(nid, 2)
									local nt = runtime.getTransform(nid)
									nt.x = na.x
									nt.y = na.y
									nt.angle = na.angle
									nt.vx = na.vx
									nt.vy = na.vy
									session.asteroids[nid] = {
										size = na.size,
										shape = na.shape,
										r = na.r,
										rotSpeed = na.rotSpeed,
									}
									runtime.addScript(nid, "Asteroid")
								end
							end
							break
						end
					end
				end
			end
		end

		do
			local deadB: { number } = {}
			for bid, b in pairs(session.bullets) do
				if b.dead then
					deadB[#deadB + 1] = bid
				end
			end
			for _, bid in ipairs(deadB) do
				destroyBullet(bid)
			end
		end

		do
			local deadA: { number } = {}
			for aid, a in pairs(session.asteroids) do
				if a.dead then
					deadA[#deadA + 1] = aid
				end
			end
			for _, aid in ipairs(deadA) do
				destroyAsteroid(aid)
			end
		end

		-- ship vs asteroid
		if shipEnt.alive and shipEnt.respawnTimer <= 0 and shipEnt.respawnDelayTimer <= 0 and shipId then
			local stt = runtime.getTransform(shipId)
			for aid, a in pairs(session.asteroids) do
				if not a.dead then
					local at = runtime.getTransform(aid)
					local r = a.r + C.SHIP_RADIUS
					if dist2(stt.x, stt.y, at.x, at.y) < r * r then
						audio.play("death")
						vfx.spawnShipBreakup(stt.x, stt.y, stt.angle, stt.vx, stt.vy)
						shipEnt.lives -= 1
						if shipEnt.lives <= 0 then
							shipEnt.alive = false
							session.state = "gameover"
						else
							stt.vx, stt.vy = 0, 0
							shipEnt.respawnDelayTimer = C.SHIP_DEATH_RESPAWN_DELAY
						end
						break
					end
				end
			end
		end

		-- wave clear
		local astCount = 0
		for _, a in pairs(session.asteroids) do
			if not a.dead then
				astCount += 1
			end
		end
		if astCount == 0 and session.state == "playing" then
			session.state = "newwave"
			session.waveDelay = 2.0
		end

		-- heartbeat
		if astCount > 0 then
			local maxAsts = (2 + session.wave) * 4
			local ratio = math.min(astCount / maxAsts, 1)
			local interval = 0.22 + ratio * 0.58
			session.beatTimer -= dt
			if session.beatTimer <= 0 then
				audio.play("beat" .. session.beatIndex)
				session.beatIndex = 1 - session.beatIndex
				session.beatTimer = interval
			end
		end
	end,
	draw = function(_id: number, _t: number) end,
}
