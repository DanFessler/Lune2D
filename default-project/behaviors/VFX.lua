--!strict
local vfx = require("game/vfx")

return {
	start = function(_self)
		vfx.clearVfx()
	end,
	update = function(_self, dt: number)
		vfx.updateParticles(dt)
	end,
	draw = function(_self, totalTime: number)
		vfx.drawParticles(totalTime)
	end,
}
