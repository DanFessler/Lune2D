--!strict
local session = require("game/session")
local C = require("game/constants")
local D = require("game/draw_util")

local function spawnBulletFromShip(self: any, sx: number, sy: number, sang: number, svx: number, svy: number)
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
	local bspd = (self :: any).bulletSpeed :: number
	t.vx = svx + math.cos(rad) * bspd
	t.vy = svy + math.sin(rad) * bspd
	local life = (self :: any).bulletLife :: number
	session.bullets[bid] = { life = life }
	runtime.addScript(bid, "Bullet")
	audio.play("shoot")
end

return {
	properties = defineProperties({
		rotateSpeed = prop.number(C.SHIP_ROT_SPD, { min = 60, max = 400, slider = true }),
		thrust = prop.number(C.SHIP_THRUST, { min = 80, max = 600, slider = true }),
		maxSpeed = prop.number(C.SHIP_MAX_SPD, { min = 150, max = 800, slider = true }),
		shootCooldown = prop.number(C.SHOOT_COOLDOWN, { min = 0.06, max = 0.8, slider = true }),
		lives = prop.integer(3, { min = 1, max = 9, slider = true }),
		bulletSpeed = prop.number(C.BULLET_SPD, { min = 250, max = 780 }),
		bulletLife = prop.number(C.BULLET_LIFE, { min = 0.25, max = 2.5 }),
		hullColor = prop.color(255, 255, 255, 255),
		flameColor = prop.color(255, 160, 0, 255),
	}),

	start = function(self)
		local id = self.entityId
		session.shipEntityId = id
		session.ship = {
			thrusting = false,
			shootTimer = 0,
			respawnDelayTimer = 0,
			respawnTimer = 0,
			alive = true,
			lives = self.lives,
		}
		local t = runtime.getTransform(id)
		t.x = screen.w / 2
		t.y = screen.h / 2
	end,
	update = function(self, dt: number)
		local entityId = self.entityId
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
				t.angle -= self.rotateSpeed * dt
			end
			if input.down("right") or input.down("d") then
				t.angle += self.rotateSpeed * dt
			end
			local thrusting = input.down("up") or input.down("w")
			ship.thrusting = thrusting
			if thrusting then
				local rad = (t.angle - 90) * C.PI / 180
				t.vx += math.cos(rad) * self.thrust * dt
				t.vy += math.sin(rad) * self.thrust * dt
			end
			audio.thrust(thrusting)
			local spd2 = t.vx * t.vx + t.vy * t.vy
			local cap = self.maxSpeed
			if spd2 > cap * cap then
				local sc = cap / math.sqrt(spd2)
				t.vx *= sc
				t.vy *= sc
			end
			t.x, t.y = D.wrap(t.x + t.vx * dt, t.y + t.vy * dt)
			if input.down("space") and ship.shootTimer <= 0 then
				ship.shootTimer = self.shootCooldown
				spawnBulletFromShip(self, t.x, t.y, t.angle, t.vx, t.vy)
			end
		end
	end,
	draw = function(self, totalTime: number)
		local ship = session.ship
		if not ship.alive or ship.respawnDelayTimer > 0 then
			return
		end
		local hull = self.hullColor :: { number }
		local flame = self.flameColor :: { number }
		local visible = ship.respawnTimer <= 0 or (math.floor(totalTime * 8) % 2 == 0)
		if visible then
			local r: number, g: number, b: number, a: number
			if ship.respawnTimer > 0 then
				r, g, b, a = 160, 160, 160, 255
			else
				r, g, b, a = hull[1], hull[2], hull[3], hull[4] or 255
			end
			D.drawPolyWrapped(C.SHIP_SHAPE, 0, 0, 0, r, g, b, a)
			if ship.thrusting and math.floor(totalTime * 20) % 2 == 0 then
				D.drawPolyWrapped(C.FLAME_SHAPE, 0, 0, 0, flame[1], flame[2], flame[3], flame[4] or 255)
			end
		end
	end,
}
