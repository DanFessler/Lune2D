--!strict
-- Transform entities: ordered draw list. Gameplay mutates entity tables;
-- world.rebuild() refreshes ordering and syncs the engine entity registry for host/tooling.

export type TransformEntity = {
	id: string,
	name: string,
	x: number,
	y: number,
	angle: number,
	vx: number,
	vy: number,
	draw: (self: TransformEntity, totalTime: number) -> (),
}

local world = {}

world._list = {} :: { TransformEntity }

function world.rebuild(ship: TransformEntity, asteroids: { any }, bullets: { any })
	local L: { TransformEntity } = {}
	-- Draw order matches gameplay: asteroids → bullets → player on top
	for _, a in ipairs(asteroids) do
		if not a.dead then
			L[#L+1] = a :: any
		end
	end
	for _, b in ipairs(bullets) do
		if not b.dead then
			L[#L+1] = b :: any
		end
	end
	L[#L+1] = ship
	world._list = L

	for _, e in ipairs(world._list) do
		engine.entities.add(e.id, e.name, e.x, e.y, e.angle, e.vx, e.vy)
	end
end

function world.draw_all(totalTime: number)
	for _, e in ipairs(world._list) do
		e.draw(e, totalTime)
	end
end

return world
