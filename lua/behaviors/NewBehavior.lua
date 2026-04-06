--!strict
local D = require("game/draw_util")

return {
	start = function(_self)
	end,

	update = function(_self, _dt: number)
	end,

	draw = function(_self, _totalTime: number)
		-- Local origin (0, 0) in entity space; follows transform from parent/scene.
		D.drawCircleWrapped(0, 0, 8, 120, 220, 255, 255)
	end,

	keydown = function(_self, _key: string)
	end,

	onHudPlay = function(_self)
	end,
}
