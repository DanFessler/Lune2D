--!strict
local Camera = {}

Camera.properties = defineProperties({
	vfov = prop.number(0),
	backgroundColor = prop.color(0, 0, 0, 255),
})

local function syncToHost(self: any)
	local v: number = self.vfov
	if v <= 0 then
		v = screen.h / runtime.getPPU()
	end
	local c = self.backgroundColor :: { number }
	runtime.setCameraProperties(v, c[1], c[2], c[3])
end

function Camera.start(self: any)
	runtime.setActiveCamera(self.entityId)
	syncToHost(self)
end

-- Inspector updates only refresh the Luau `self` table; keep C++ camera state in sync every frame
-- while this entity is the active play camera (matches other behaviors that read `self` in update).
function Camera.update(self: any, _dt: number)
	local ps = editor.playState()
	if ps == "stopped" then
		return
	end
	if runtime.getActiveCameraEntityId() ~= self.entityId then
		return
	end
	syncToHost(self)
end

return Camera
