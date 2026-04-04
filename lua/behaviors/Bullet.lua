--!strict
local session = require("game/session")
local C = require("game/constants")
local D = require("game/draw_util")

local function destroyBullet(id: number)
	session.bullets[id] = nil
	runtime.destroy(id)
end

return {
	start = function(_id: number) end,
	update = function(entityId: number, dt: number)
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
	draw = function(entityId: number, _t: number)
		local ex = session.bullets[entityId]
		if not ex or ex.dead then
			return
		end
		D.drawCircleWrapped(0, 0, C.BULLET_RADIUS, 0, 220, 255, 255)
	end,
}
