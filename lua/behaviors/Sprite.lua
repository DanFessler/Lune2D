--!strict
-- Engine-provided sprite behavior (see AGENTS.md: `lua/behaviors/`).
-- Requires Transform on the same entity; draw runs in entity-local space with world matrix applied by the host.

local function propVec2AsArray(v: any): { number }
	if type(v) == "table" and v[1] ~= nil and v[2] ~= nil then
		return { v[1] :: number, v[2] :: number }
	end
	return { 0.5, 0.5 }
end

return {
	properties = defineProperties({
		image = prop.asset(""),
		width = prop.number(64, { min = 1, max = 4096, slider = true }),
		height = prop.number(64, { min = 1, max = 4096, slider = true }),
		origin = prop.vector(0.5, 0.5),
		tint = prop.color(255, 255, 255, 255),
	}),

	draw = function(self, _totalTime: number)
		if self.image == "" then
			return
		end
		local o = propVec2AsArray(self.origin)
		local ox = -o[1] * self.width
		local oy = -o[2] * self.height
		local a = self.tint[4]
		draw.sprite(
			self.image,
			ox,
			oy,
			self.width,
			self.height,
			self.tint[1],
			self.tint[2],
			self.tint[3],
			if type(a) == "number" then a else 255
		)
	end,
}
