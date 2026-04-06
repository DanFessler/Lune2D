--!strict
local session = require("game/session")
local D = require("game/draw_util")

return {
	properties = defineProperties {
		tint = prop.color(255, 255, 255, 255),
		rotationMultiplier = prop.number(1, { min = -3, max = 3 }),
	},

	start = function(_self) end,
	update = function(self, dt: number)
		local entityId = self.entityId
		local st = session.state
		if st == "title" or st == "gameover" then
			return
		end
		local a = session.asteroids[entityId]
		if not a or a.dead then
			return
		end
		local t = runtime.getTransform(entityId)
		t.x, t.y = D.wrap(t.x + t.vx * dt, t.y + t.vy * dt)
		t.angle += a.rotSpeed * self.rotationMultiplier * dt
	end,
	draw = function(self, _t: number)
		local entityId = self.entityId
		local a = session.asteroids[entityId]
		if not a or a.dead then
			return
		end
		local c = self.tint :: { number }
		D.drawPolyWrapped(a.shape, 0, 0, 0, c[1], c[2], c[3], c[4] or 255)
	end,
}
