--!strict
local session = require("game/session")
local D = require("game/draw_util")

return {
	start = function(_id: number) end,
	update = function(entityId: number, dt: number)
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
		t.angle += a.rotSpeed * dt
	end,
	draw = function(entityId: number, _t: number)
		local a = session.asteroids[entityId]
		if not a or a.dead then
			return
		end
		D.drawPolyWrapped(a.shape, 0, 0, 0, 255, 255, 255, 255)
	end,
}
