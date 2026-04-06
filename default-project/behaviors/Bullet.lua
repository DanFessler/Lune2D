--!strict
local session = require("game/session")
local C = require("game/constants")
local D = require("game/draw_util")

local function destroyBullet(id: number)
	session.bullets[id] = nil
	runtime.destroy(id)
end

return {
	properties = defineProperties {
		tint = prop.color(0, 220, 255, 255),
	},

	start = function(_self) end,
	update = function(self, dt: number)
		local entityId = self.entityId
		local st = session.state
		if st == "title" or st == "gameover" then
			return
		end
		local ex = session.bullets[entityId]
		if not ex or ex.dead then
			return
		end
		ex.life -= dt
		if ex.life <= 0 then
			destroyBullet(entityId)
			return
		end
		local t = runtime.getTransform(entityId)
		t.x, t.y = D.wrap(t.x + t.vx * dt, t.y + t.vy * dt)
	end,
	draw = function(self, _t: number)
		local entityId = self.entityId
		local ex = session.bullets[entityId]
		if not ex or ex.dead then
			return
		end
		local c = self.tint :: { number }
		D.drawCircleWrapped(0, 0, C.BULLET_RADIUS, c[1], c[2], c[3], c[4] or 255)
	end,
}
