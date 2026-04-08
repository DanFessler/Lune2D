--!strict
-- Built-in editor behavior for Transform: origin dots, gizmo axes, rotation arc, selection, drag.
-- Driven by `editorInput` polling API; native no longer owns pick/drag policy.
-- All picking and drawing operates in world coordinates; screen-pixel constants are divided by
-- camera scale so gizmos remain constant size on screen regardless of zoom.

local RAD = math.pi / 180
-- Base constants in screen pixels; divided by cameraScale at use sites.
local HIT_RADIUS_PX = 18
local GIZMO_AXIS_LEN_PX = 36
local AXIS_HIT_PX = 14
local AXIS_T_MIN = 0.22
local GIZMO_ROT_ARC_SPAN = math.pi / 8
local ROT_ARC_HIT_RADIAL_PX = 10
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

local function getCameraScale(): number
	local cam = runtime.getEditorCamera()
	local vfov = cam.vfov
	if vfov < 1 then
		vfov = screen.h / runtime.getPPU()
	end
	return screen.h / vfov
end

local function gizmoAxisTips(ox: number, oy: number, rad: number, axisLen: number): (number, number, number, number)
	local c = math.cos(rad)
	local s = math.sin(rad)
	local x2 = ox + c * axisLen
	local y2 = oy + s * axisLen
	local x3 = ox - s * axisLen
	local y3 = oy + c * axisLen
	return x2, y2, x3, y3
end

local function tryGizmoAxisHit(sel: number, mx: number, my: number, camScale: number): ("axis_x" | "axis_y")?
	local wt = runtime.getWorldTransform(sel)
	if not wt then
		return nil
	end
	local axisLen = GIZMO_AXIS_LEN_PX / camScale
	local axisHit = AXIS_HIT_PX / camScale
	local rad = wt.angle * RAD
	local c = math.cos(rad)
	local s = math.sin(rad)
	local ox, oy = wt.x, wt.y
	local x2 = ox + c * axisLen
	local y2 = oy + s * axisLen
	local x3 = ox - s * axisLen
	local y3 = oy + c * axisLen

	local dX, tX = distPointToSegment(mx, my, ox, oy, x2, y2)
	local dY, tY = distPointToSegment(mx, my, ox, oy, x3, y3)

	local hitX = tX >= AXIS_T_MIN and dX <= axisHit
	local hitY = tY >= AXIS_T_MIN and dY <= axisHit
	if hitX and hitY then
		return if dX <= dY then "axis_x" else "axis_y"
	elseif hitX then
		return "axis_x"
	elseif hitY then
		return "axis_y"
	end
	return nil
end

local function tryGizmoRotateHit(sel: number, mx: number, my: number, camScale: number): boolean
	local wt = runtime.getWorldTransform(sel)
	if not wt then
		return false
	end
	local axisLen = GIZMO_AXIS_LEN_PX / camScale
	local radialHit = ROT_ARC_HIT_RADIAL_PX / camScale
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
	return math.abs(rm - axisLen) <= radialHit
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
	local mx, my = runtime.screenToWorld(pos.x, pos.y)
	local camScale = getCameraScale()
	local hitRadius = HIT_RADIUS_PX / camScale

	if editorInput.mousePressed(0) then
		local sel = editor.getSelectedEntityId()
		local pickedProbe = editor.pickEntityAt(mx, my, hitRadius)
		local axisHit: ("axis_x" | "axis_y")? = nil
		local rotateHit = false
		if sel ~= 0 and (pickedProbe == 0 or pickedProbe == sel) then
			axisHit = tryGizmoAxisHit(sel, mx, my, camScale)
			if axisHit == nil then
				rotateHit = tryGizmoRotateHit(sel, mx, my, camScale)
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
	local camScale = getCameraScale()
	local axisLen = GIZMO_AXIS_LEN_PX / camScale
	local dotRadius = 6 / camScale
	local tipSize = 5 / camScale

	local sel = editor.getSelectedEntityId()
	runtime.forEachEntityDrawOrder(function(entityId: number)
		local wt = runtime.getWorldTransform(entityId)
		local br, bg, bb, ba = 235, 235, 245, 255
		if sel == entityId then
			br, bg, bb, ba = 90, 255, 120, 255
		end
		draw.circle(wt.x, wt.y, dotRadius, br, bg, bb, ba)

		if sel == entityId then
			local rad = wt.angle * RAD
			local c = math.cos(rad)
			local s = math.sin(rad)
			local ox, oy = wt.x, wt.y
			local x2, y2, x3, y3 = gizmoAxisTips(ox, oy, rad, axisLen)
			draw.line(ox, oy, x2, y2, 255, 90, 90, 255)
			draw.line(ox, oy, x3, y3, 90, 200, 255, 255)
			draw.line(x2 - tipSize * s, y2 + tipSize * c, x2 + tipSize * s, y2 - tipSize * c, 255, 90, 90, 255)
			draw.line(x3 - tipSize * c, y3 - tipSize * s, x3 + tipSize * c, y3 + tipSize * s, 90, 200, 255, 255)

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
