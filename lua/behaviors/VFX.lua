--!strict
local vfx = require("game/vfx")

return {
	start = function(_id: number)
		vfx.clearVfx()
	end,
	update = function(_id: number, dt: number)
		vfx.updateParticles(dt)
	end,
	draw = function(_id: number, totalTime: number)
		vfx.drawParticles(totalTime)
	end,
}
