--!strict
-- Editor camera controller: pan (middle-drag, arrow keys), wheel zoom, Shift+↑/↓ zoom.
-- Ported from ctx-game EditorCameraController.ts.

-- Keyboard pan: world-units/sec (integrated with `* dt`). Middle-drag still uses per-frame deltas.
local panAccel: number = 4200
local maxPanSpeed: number = 9000
local friction: number = 0.9

local scrollImpulsePerTick: number = 14
local scrollMaxSpeed: number = 70
local scrollFriction: number = 0.82
-- Shift + Up/Down: ramp scrollVelocity (same sign convention as wheel).
local keyZoomSpeed: number = 140

local minVfov: number = 1
local maxVfov: number = 10000

local vx: number = 0
local vy: number = 0
local scrollVelocity: number = 0
local isDragging: boolean = false
local lastDragX: number = 0
local lastDragY: number = 0

local function clamp(v: number, lo: number, hi: number): number
	if v < lo then return lo end
	if v > hi then return hi end
	return v
end

local function frictionAtDt(base: number, dt: number): number
	return math.pow(base, dt * 60)
end

local function updateWorld(dt: number)
	if not editorInput then
		return
	end

	local cam = runtime.getEditorCamera()
	local camX: number = cam.x
	local camY: number = cam.y
	local camAngle: number = cam.angle
	local vfov: number = cam.vfov
	if vfov < 1 then
		vfov = screen.h / runtime.getPPU()
	end

	local cameraScale: number = screen.h / vfov
	local shiftDown: boolean = input.down("shift")

	local pos = editorInput.mousePosition()
	local mx: number = pos.x
	local my: number = pos.y

	-- Middle-mouse drag to pan
	if editorInput.mousePressed(1) then
		isDragging = true
		lastDragX = mx
		lastDragY = my
	end
	if not editorInput.mouseDown(1) and isDragging then
		isDragging = false
	end

	if isDragging then
		local dx: number = -(mx - lastDragX) / cameraScale
		local dy: number = -(my - lastDragY) / cameraScale
		lastDragX = mx
		lastDragY = my
		camX = camX + dx
		camY = camY + dy
		-- Don't mix keyboard velocity with drag; release arrow momentum while panning.
		vx = 0
		vy = 0
	else
		-- Arrow keys: accumulate velocity (world units/s), integrate with dt.
		if input.down("left") then
			vx = vx - panAccel * dt
		end
		if input.down("right") then
			vx = vx + panAccel * dt
		end
		if not shiftDown then
			if input.down("up") then
				vy = vy - panAccel * dt
			end
			if input.down("down") then
				vy = vy + panAccel * dt
			end
		end
		local drag: number = frictionAtDt(friction, dt)
		vx = vx * drag
		vy = vy * drag
		vx = clamp(vx, -maxPanSpeed, maxPanSpeed)
		vy = clamp(vy, -maxPanSpeed, maxPanSpeed)
		camX = camX + vx * dt
		camY = camY + vy * dt
	end

	-- Scroll wheel zoom (delta was cleared before editor ran; see native input order).
	local scrollDelta: number = editorInput.scrollDelta()
	if scrollDelta ~= 0 then
		scrollVelocity = scrollVelocity + scrollDelta * scrollImpulsePerTick
	end
	if shiftDown then
		if input.down("up") then
			scrollVelocity = scrollVelocity - keyZoomSpeed * dt
		end
		if input.down("down") then
			scrollVelocity = scrollVelocity + keyZoomSpeed * dt
		end
	end
	scrollVelocity = scrollVelocity * frictionAtDt(scrollFriction, dt)
	scrollVelocity = clamp(scrollVelocity, -scrollMaxSpeed, scrollMaxSpeed)

	-- Apply zoom (mouse-anchored)
	if math.abs(scrollVelocity) > 0.001 then
		local wx0, wy0 = runtime.screenToWorld(mx, my)

		vfov = vfov * (1 + scrollVelocity * dt)
		vfov = clamp(vfov, minVfov, maxVfov)

		-- Commit the new vfov so screenToWorld uses the updated projection
		runtime.setEditorCamera(camX, camY, camAngle, vfov)

		local wx1, wy1 = runtime.screenToWorld(mx, my)

		camX = camX - (wx1 - wx0)
		camY = camY - (wy1 - wy0)
	end

	runtime.setEditorCamera(camX, camY, camAngle, vfov)
end

return { updateWorld = updateWorld }
