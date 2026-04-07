--!strict
-- Editor overlay: sprite bounds in entity-local space (matches `lua/behaviors/Sprite.lua` layout).

local function propVec2AsArray(v: any): { number }
	if type(v) == "table" and v[1] ~= nil and v[2] ~= nil then
		return { v[1] :: number, v[2] :: number }
	end
	return { 0.5, 0.5 }
end

return {
	draw = function(self, _totalTime: number)
		local w, h = self.width, self.height
		if w < 1 or h < 1 then
			return
		end
		local o = propVec2AsArray(self.origin)
		local ox = -o[1] * w
		local oy = -o[2] * h
		draw.poly(
			{ { 0, 0 }, { w, 0 }, { w, h }, { 0, h } },
			ox,
			oy,
			0,
			120,
			200,
			255,
			200
		)
	end,
}
