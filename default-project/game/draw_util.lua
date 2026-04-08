--!strict
-- Immediate-mode helpers using global `screen` and `draw`.

type Polygon = { { number } }

local function drawPolyWrapped(shape: Polygon, lx: number, ly: number, lAngle: number,
	r: number, g: number, b: number, a: number)
	local W, H = screen.w, screen.h
	for ox = -W, W, W do
		for oy = -H, H, H do
			draw.pushMatrix()
			draw.translateScreen(ox, oy)
			draw.poly(shape, lx, ly, lAngle, r, g, b, a)
			draw.popMatrix()
		end
	end
end

local function drawCircleWrapped(cx: number, cy: number, radius: number,
	r: number, g: number, b: number, a: number)
	local W, H = screen.w, screen.h
	for ox = -W, W, W do
		for oy = -H, H, H do
			draw.pushMatrix()
			draw.translateScreen(ox, oy)
			draw.circle(cx, cy, radius, r, g, b, a)
			draw.popMatrix()
		end
	end
end

local function drawLineWrapped(x1: number, y1: number, x2: number, y2: number,
	cr: number, cg: number, cb: number, ca: number)
	local W, H = screen.w, screen.h
	for ox = -W, W, W do
		for oy = -H, H, H do
			draw.pushMatrix()
			draw.translateScreen(ox, oy)
			draw.line(x1, y1, x2, y2, cr, cg, cb, ca)
			draw.popMatrix()
		end
	end
end

-- Torus wrap on a playfield centered at (0,0): x in (-W/2, W/2], y in (-H/2, H/2].
local function wrap(x: number, y: number): (number, number)
	local W, H = screen.w, screen.h
	local hw, hh = W / 2, H / 2
	while x <= -hw do
		x += W
	end
	while x > hw do
		x -= W
	end
	while y <= -hh do
		y += H
	end
	while y > hh do
		y -= H
	end
	return x, y
end

return {
	drawPolyWrapped = drawPolyWrapped,
	drawCircleWrapped = drawCircleWrapped,
	drawLineWrapped = drawLineWrapped,
	wrap = wrap,
}
