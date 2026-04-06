--!strict
-- Built-in world-space transform handles. Loaded automatically from lua/editor/;
-- not attached as a gameplay Script — kept in Luau like other editor behaviors.
--
-- `draw.*` color components are 0–255 integers (same as gameplay / game/vfx.lua).

local RAD = math.pi / 180

local function drawWorld(_totalTime: number)
	draw.resetMatrix()
	local sel = editor.getSelectedEntityId()
	runtime.forEachEntityDrawOrder(function(entityId: number)
		local wt = runtime.getWorldTransform(entityId)
		local r = 6
		local br, bg, bb, ba = 235, 235, 245, 255
		if sel == entityId then
			br, bg, bb, ba = 90, 255, 120, 255
		end
		draw.circle(wt.x, wt.y, r, br, bg, bb, ba)

		if sel == entityId then
			local axisLen = 36
			local rad = wt.angle * RAD
			local c = math.cos(rad)
			local s = math.sin(rad)
			-- Local +X / +Y in world space (matches entity rotation).
			local x2 = wt.x + c * axisLen
			local y2 = wt.y + s * axisLen
			local x3 = wt.x - s * axisLen
			local y3 = wt.y + c * axisLen
			draw.line(wt.x, wt.y, x2, y2, 255, 90, 90, 255)
			draw.line(wt.x, wt.y, x3, y3, 90, 200, 255, 255)
			-- Small cross at axis tips for visibility.
			local tip = 5
			draw.line(x2 - tip * s, y2 + tip * c, x2 + tip * s, y2 - tip * c, 255, 90, 90, 255)
			draw.line(x3 - tip * c, y3 - tip * s, x3 + tip * c, y3 + tip * s, 90, 200, 255, 255)
		end
	end)
end

return {
	drawWorld = drawWorld,
}
