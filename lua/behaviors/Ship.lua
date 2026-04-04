--!strict
local session = require("game/session")
local C = require("game/constants")
local D = require("game/draw_util")

local function spawnBulletFromShip(sx: number, sy: number, sang: number, svx: number, svy: number)
	local rad = (sang - 90) * C.PI / 180
	local tx = sx + math.cos(rad) * C.SHIP_RADIUS * 1.3
	local ty = sy + math.sin(rad) * C.SHIP_RADIUS * 1.3
	local bid = runtime.spawn("Bullet")
	runtime.setDrawOrder(bid, 1)
	runtime.setUpdateOrder(bid, 1)
	local t = runtime.getTransform(bid)
	t.x = tx
	t.y = ty
	t.angle = 0
	t.vx = svx + math.cos(rad) * C.BULLET_SPD
	t.vy = svy + math.sin(rad) * C.BULLET_SPD
	session.bullets[bid] = { life = C.BULLET_LIFE }
	runtime.addScript(bid, "Bullet")
	audio.play("shoot")
end

return {
	start = function(id: number)
		session.shipEntityId = id
		session.ship = {
			thrusting = false,
			shootTimer = 0,
			respawnDelayTimer = 0,
			respawnTimer = 0,
			alive = true,
			lives = 3,
		}
		local t = runtime.getTransform(id)
		t.x = screen.w / 2
		t.y = screen.h / 2
	end,
	update = function(entityId: number, dt: number)
		local st = session.state
		if st == "title" or st == "gameover" then
			session.ship.thrusting = false
			return
		end
		local ship = session.ship
		ship.shootTimer -= dt
		local t = runtime.getTransform(entityId)
		local justWarped = false
		if ship.respawnDelayTimer > 0 then
			ship.respawnDelayTimer -= dt
			if ship.respawnDelayTimer <= 0 then
				ship.respawnDelayTimer = 0
				t.x, t.y = screen.w / 2, screen.h / 2
				t.vx, t.vy = 0, 0
				t.angle = 270
				ship.respawnTimer = C.SHIP_POST_RESPAWN_INVULN
				justWarped = true
			else
				session.ship.thrusting = false
				audio.thrust(false)
				return
			end
		end
		if ship.respawnTimer > 0 and not justWarped then
			ship.respawnTimer -= dt
		end
		if ship.alive then
			if input.down("left") or input.down("a") then
				t.angle -= C.SHIP_ROT_SPD * dt
			end
			if input.down("right") or input.down("d") then
				t.angle += C.SHIP_ROT_SPD * dt
			end
			local thrusting = input.down("up") or input.down("w")
			ship.thrusting = thrusting
			if thrusting then
				local rad = (t.angle - 90) * C.PI / 180
				t.vx += math.cos(rad) * C.SHIP_THRUST * dt
				t.vy += math.sin(rad) * C.SHIP_THRUST * dt
			end
			audio.thrust(thrusting)
			local spd2 = t.vx * t.vx + t.vy * t.vy
			if spd2 > C.SHIP_MAX_SPD * C.SHIP_MAX_SPD then
				local sc = C.SHIP_MAX_SPD / math.sqrt(spd2)
				t.vx *= sc
				t.vy *= sc
			end
			t.x, t.y = D.wrap(t.x + t.vx * dt, t.y + t.vy * dt)
			if input.down("space") and ship.shootTimer <= 0 then
				ship.shootTimer = C.SHOOT_COOLDOWN
				spawnBulletFromShip(t.x, t.y, t.angle, t.vx, t.vy)
			end
		end
	end,
	draw = function(_entityId: number, totalTime: number)
		local ship = session.ship
		if not ship.alive or ship.respawnDelayTimer > 0 then
			return
		end
		local visible = ship.respawnTimer <= 0 or (math.floor(totalTime * 8) % 2 == 0)
		if visible then
			local r: number, g: number, b: number
			if ship.respawnTimer > 0 then
				r, g, b = 160, 160, 160
			else
				r, g, b = 255, 255, 255
			end
			D.drawPolyWrapped(C.SHIP_SHAPE, 0, 0, 0, r, g, b, 255)
			if ship.thrusting and math.floor(totalTime * 20) % 2 == 0 then
				D.drawPolyWrapped(C.FLAME_SHAPE, 0, 0, 0, 255, 160, 0, 255)
			end
		end
	end,
}
