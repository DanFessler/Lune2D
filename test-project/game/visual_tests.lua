-- Pixel-probe harness for `--run-visual-tests` (invoked from native after a full draw).

local function near(r: number, g: number, b: number, er: number, eg: number, eb: number, tol: number): boolean
	if math.abs(r - er) > tol then
		return false
	end
	if math.abs(g - eg) > tol then
		return false
	end
	if math.abs(b - eb) > tol then
		return false
	end
	return true
end

local function probe(name: string, x: number, y: number, er: number, eg: number, eb: number): boolean
	local pr, pg, pb, _pa = screen.readPixel(x, y)
	if pr == nil then
		print("visual_tests FAIL:", name, "readPixel failed at", x, y)
		return false
	end
	if not near(pr :: number, pg :: number, pb :: number, er, eg, eb, 14) then
		print("visual_tests FAIL:", name, "expected", er, eg, eb, "got", pr, pg, pb, "at", x, y)
		return false
	end
	return true
end

local function run(): boolean
	local w = screen.w
	local h = screen.h
	if w < 64 or h < 64 then
		print("visual_tests FAIL: screen too small", w, h)
		return false
	end
	local qx = math.min(w - 2, math.max(2, math.floor(w / 4)))
	local qy = math.floor(h / 2)
	if not probe("red_hline", qx, qy, 255, 0, 0) then
		return false
	end
	local gx = math.floor(w / 2)
	local gy = math.min(h - 2, math.max(2, math.floor(h / 4)))
	if not probe("green_vline", gx, gy, 0, 255, 0) then
		return false
	end
	if not probe("blue_block", 5, 5, 0, 120, 255) then
		return false
	end
	if not probe("white_stroke", 100, 100, 255, 255, 255) then
		return false
	end
	-- Cleared with Camera.backgroundColor (see default.scene.json), not default black.
	if not probe("bg_corner", w - 2, h - 2, 18, 52, 86) then
		return false
	end
	return true
end

return { run = run }
