--!strict
-- Built-in editor behavior for Transform: origin dots, gizmo axes, rotation arc, selection, drag.
-- Driven by `editorInput` polling API; native no longer owns pick/drag policy.

local RAD = math.pi / 180
local HIT_RADIUS = 18
local GIZMO_AXIS_LEN = 36
-- Luau-space half width for hitting an axis segment (thick pick region).
local AXIS_HIT = 14
-- Only treat as axis if click projects this far along the arrow (origin dot keeps free XY drag).
local AXIS_T_MIN = 0.22
-- Rotation hint arc: short span centered on local +X/+Y bisector (rad + π/4), not a full quarter turn.
local GIZMO_ROT_ARC_SPAN = math.pi / 8
-- Pick tolerance: radial distance from gizmo circle; mouse must lie within arc angular span.
local ROT_ARC_HIT_RADIAL = 10
local ROT_ARC_HIT_ANG = 0.06

local function distPointToSegment(px: number, py: number, ax: number, ay: number, bx: number, by: number): (number, number)
	local abx = bx - ax
	local aby = by - ay
	local ab2 = abx * abx + aby * aby
	if ab2 < 1e-8 then
		local dx = px - ax
		local dy = py - ay
		return math.sqrt(dx * dx + dy * dy), 0
	end
	local apx = px - ax
	local apy = py - ay
	local t = (apx * abx + apy * aby) / ab2
	t = math.max(0, math.min(1, t))
	local cx = ax + t * abx
	local cy = ay + t * aby
	local dx = px - cx
	local dy = py - cy
	return math.sqrt(dx * dx + dy * dy), t
end

local function unwrapAngleRad(delta: number): number
	local twoPi = 2 * math.pi
	while delta > math.pi do
		delta -= twoPi
	end
	while delta < -math.pi do
		delta += twoPi
	end
	return delta
end

-- Axis arrow tip endpoints (world space).
local function gizmoAxisTips(ox: number, oy: number, rad: number): (number, number, number, number)
	local L = GIZMO_AXIS_LEN
	local c = math.cos(rad)
	local s = math.sin(rad)
	local x2 = ox + c * L
	local y2 = oy + s * L
	local x3 = ox - s * L
	local y3 = oy + c * L
	return x2, y2, x3, y3
end

local function tryGizmoAxisHit(sel: number, mx: number, my: number): ("axis_x" | "axis_y")?
	local wt = runtime.getWorldTransform(sel)
	if not wt then
		return nil
	end
	local rad = wt.angle * RAD
	local c = math.cos(rad)
	local s = math.sin(rad)
	local ox, oy = wt.x, wt.y
	local x2 = ox + c * GIZMO_AXIS_LEN
	local y2 = oy + s * GIZMO_AXIS_LEN
	local x3 = ox - s * GIZMO_AXIS_LEN
	local y3 = oy + c * GIZMO_AXIS_LEN

	local dX, tX = distPointToSegment(mx, my, ox, oy, x2, y2)
	local dY, tY = distPointToSegment(mx, my, ox, oy, x3, y3)

	local hitX = tX >= AXIS_T_MIN and dX <= AXIS_HIT
	local hitY = tY >= AXIS_T_MIN and dY <= AXIS_HIT
	if hitX and hitY then
		return if dX <= dY then "axis_x" else "axis_y"
	elseif hitX then
		return "axis_x"
	elseif hitY then
		return "axis_y"
	end
	return nil
end

local function tryGizmoRotateHit(sel: number, mx: number, my: number): boolean
	local wt = runtime.getWorldTransform(sel)
	if not wt then
		return false
	end
	local ox, oy = wt.x, wt.y
	local rad = wt.angle * RAD
	local aMidArc = rad + math.pi / 4
	local halfArc = GIZMO_ROT_ARC_SPAN * 0.5
	local dx = mx - ox
	local dy = my - oy
	local rm = math.sqrt(dx * dx + dy * dy)
	if rm < 1e-4 then
		return false
	end
	local dAng = math.abs(unwrapAngleRad(math.atan2(dy, dx) - aMidArc))
	if dAng > halfArc + ROT_ARC_HIT_ANG then
		return false
	end
	return math.abs(rm - GIZMO_AXIS_LEN) <= ROT_ARC_HIT_RADIAL
end

-- Editor interaction state machine.
local state: "idle" | "dragging_free" | "dragging_axis_x" | "dragging_axis_y" | "dragging_rotate" = "idle"
local dragEntityId: number = 0
local dragStartLx: number = 0
local dragStartLy: number = 0

local function updateWorld(_dt: number)
	if not editorInput then
		return
	end

	local pos = editorInput.mousePosition()
	local mx, my = pos.x, pos.y

	if editorInput.mousePressed(0) then
		local sel = editor.getSelectedEntityId()
		local pickedProbe = editor.pickEntityAt(mx, my, HIT_RADIUS)
		local axisHit: ("axis_x" | "axis_y")? = nil
		local rotateHit = false
		-- Axes first so fat axis slubs win over the rotation arc hit band.
		if sel ~= 0 and (pickedProbe == 0 or pickedProbe == sel) then
			axisHit = tryGizmoAxisHit(sel, mx, my)
			if axisHit == nil then
				rotateHit = tryGizmoRotateHit(sel, mx, my)
			end
		end

		if axisHit == "axis_x" then
			editor.setSelectedEntity(sel)
			state = "dragging_axis_x"
			dragEntityId = sel
			dragStartLx = mx
			dragStartLy = my
		elseif axisHit == "axis_y" then
			editor.setSelectedEntity(sel)
			state = "dragging_axis_y"
			dragEntityId = sel
			dragStartLx = mx
			dragStartLy = my
		elseif rotateHit then
			editor.setSelectedEntity(sel)
			state = "dragging_rotate"
			dragEntityId = sel
			dragStartLx = mx
			dragStartLy = my
		else
			local picked = pickedProbe
			editor.setSelectedEntity(picked)
			if picked ~= 0 then
				state = "dragging_free"
				dragEntityId = picked
				dragStartLx = mx
				dragStartLy = my
			else
				state = "idle"
				dragEntityId = 0
			end
		end
	elseif state == "dragging_free" and editorInput.mouseDown(0) then
		if dragEntityId ~= 0 then
			local dx = mx - dragStartLx
			local dy = my - dragStartLy
			-- Mouse is in world/Luau space; Transform x/y are in parent local space.
			local dlx, dly = runtime.worldDeltaToLocal(dragEntityId, dx, dy)
			local t = runtime.getTransform(dragEntityId)
			if t then
				runtime.setTransform(dragEntityId, "x", t.x + dlx)
				runtime.setTransform(dragEntityId, "y", t.y + dly)
			end
			dragStartLx = mx
			dragStartLy = my
		end
	elseif (state == "dragging_axis_x" or state == "dragging_axis_y") and editorInput.mouseDown(0) then
		if dragEntityId ~= 0 then
			local dx = mx - dragStartLx
			local dy = my - dragStartLy
			local wt = runtime.getWorldTransform(dragEntityId)
			local t = runtime.getTransform(dragEntityId)
			if wt and t then
				local rad = wt.angle * RAD
				local c = math.cos(rad)
				local s = math.sin(rad)
				local wdx: number
				local wdy: number
				if state == "dragging_axis_x" then
					local proj = dx * c + dy * s
					wdx = proj * c
					wdy = proj * s
				else
					local vx = -s
					local vy = c
					local proj = dx * vx + dy * vy
					wdx = proj * vx
					wdy = proj * vy
				end
				local dlx, dly = runtime.worldDeltaToLocal(dragEntityId, wdx, wdy)
				runtime.setTransform(dragEntityId, "x", t.x + dlx)
				runtime.setTransform(dragEntityId, "y", t.y + dly)
			end
			dragStartLx = mx
			dragStartLy = my
		end
	elseif state == "dragging_rotate" and editorInput.mouseDown(0) then
		if dragEntityId ~= 0 then
			local wt = runtime.getWorldTransform(dragEntityId)
			local t = runtime.getTransform(dragEntityId)
			if wt and t then
				local ox, oy = wt.x, wt.y
				local a0 = math.atan2(dragStartLy - oy, dragStartLx - ox)
				local a1 = math.atan2(my - oy, mx - ox)
				local dRad = unwrapAngleRad(a1 - a0)
				runtime.setTransform(dragEntityId, "angle", t.angle + math.deg(dRad))
			end
			dragStartLx = mx
			dragStartLy = my
		end
	elseif editorInput.mouseReleased(0) then
		state = "idle"
		dragEntityId = 0
	end
end

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
			local axisLen = GIZMO_AXIS_LEN
			local rad = wt.angle * RAD
			local c = math.cos(rad)
			local s = math.sin(rad)
			local ox, oy = wt.x, wt.y
			local x2, y2, x3, y3 = gizmoAxisTips(ox, oy, rad)
			draw.line(ox, oy, x2, y2, 255, 90, 90, 255)
			draw.line(ox, oy, x3, y3, 90, 200, 255, 255)
			local tip = 5
			draw.line(x2 - tip * s, y2 + tip * c, x2 + tip * s, y2 - tip * c, 255, 90, 90, 255)
			draw.line(x3 - tip * c, y3 - tip * s, x3 + tip * c, y3 + tip * s, 90, 200, 255, 255)

			-- Short arc at gizmo radius, centered on local +X/+Y bisector.
			local rh, gh, bh = 255, 210, 70
			local steps = math.max(8, math.floor(20 * (GIZMO_ROT_ARC_SPAN / (math.pi / 2))))
			local aMidArc = rad + math.pi / 4
			local halfArc = GIZMO_ROT_ARC_SPAN * 0.5
			local a0 = aMidArc - halfArc
			local a1 = aMidArc + halfArc
			local px = ox + math.cos(a0) * axisLen
			local py = oy + math.sin(a0) * axisLen
			for i = 1, steps do
				local u = i / steps
				local a = a0 + (a1 - a0) * u
				local nx = ox + math.cos(a) * axisLen
				local ny = oy + math.sin(a) * axisLen
				draw.line(px, py, nx, ny, rh, gh, bh, 140)
				px = nx
				py = ny
			end
		end
	end)
end

return {
	updateWorld = updateWorld,
	drawWorld = drawWorld,
}
