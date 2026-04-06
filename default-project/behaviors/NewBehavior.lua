--!strict
local D = require("game/draw_util")

return {
	properties = defineProperties({
		radius = prop.number(8, { min = 1, max = 100, slider = true }),
		color = prop.color(255, 255, 255, 255),
	}),

	start = function(_self) end,

	update = function(_self, _dt: number) end,

	draw = function(self, _totalTime: number)
		-- Local origin (0, 0) in entity space; follows transform from parent/scene.
		D.drawCircleWrapped(0, 0, self.radius, self.color[1], self.color[2], self.color[3], self.color[4] or 255)
	end,

	keydown = function(_self, _key: string) end,

	onHudPlay = function(_self) end,
}
