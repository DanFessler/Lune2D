--!strict
-- Visual harness: draws predictable patterns for screen.readPixel probes (default editor camera).
local CameraTest = {}

function CameraTest.draw(_self: any, _t: number)
	local w = screen.w
	local h = screen.h
	local hy = math.floor(h / 2)
	local wx = math.floor(w / 2)
	draw.line(0, hy, w, hy, 255, 0, 0, 255)
	draw.line(wx, 0, wx, h, 0, 255, 0, 255)
	for yy = 0, 9 do
		draw.line(0, yy, 10, yy, 0, 120, 255, 255)
	end
	draw.line(100, 100, 101, 100, 255, 255, 255, 255)
end

return CameraTest
