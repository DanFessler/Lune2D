--!strict
local session = require("game/session")
local C = require("game/constants")

local fps: number = 0
local fpsFrames: number = 0
local fpsAccum: number = 0

local function startGame()
	if session.state == "gameover" then
		runtime.loadScene("scenes/default.scene.json")
		return
	end
	if session.state == "title" then
		-- First wave spawns immediately; waveDelay is only for between-wave gaps (GameDirector).
		session.state = "newwave"
		session.waveDelay = 0
	end
end

	return {
	properties = defineProperties {
		showFps = true,
		scoreScale = prop.number(2.5, { min = 1, max = 6, slider = true }),
		livesIconScale = prop.number(1, { min = 0.5, max = 2, slider = true }),
	},

	start = function(_self)
		fps = 0
		fpsFrames = 0
		fpsAccum = 0
	end,
	update = function(_self, dt: number)
		fpsAccum += dt
		fpsFrames += 1
		if fpsAccum >= 0.5 then
			fps = math.floor(fpsFrames / fpsAccum + 0.5)
			fpsFrames = 0
			fpsAccum = 0
		end
	end,
	draw = function(self, _totalTime: number)
		local W, H = screen.w, screen.h
		local scoreSc = self.scoreScale :: number
		local lifeSc = self.livesIconScale :: number

		draw.number(session.score, 16, 16, scoreSc, 255, 255, 255, 255)

		local ship = session.ship
		local lifeStep = 28 * lifeSc
		local lifeX0 = W - 30 * lifeSc
		local lifeY = 20
		for i = 1, ship.lives do
			draw.poly(C.MINI_SHIP, lifeX0 - (i - 1) * lifeStep, lifeY, 270, 255, 255, 255, 255)
		end

		if session.state == "title" then
			local msg = "READY"
			local scale = 4
			local cw = 8 * scale
			local sx = (W - #msg * cw) / 2
			for i = 1, #msg do
				draw.char(msg:sub(i, i), sx + (i - 1) * cw, H / 2 - 50, scale, 200, 220, 255, 255)
			end
			local pr = "PRESS SPACE TO START"
			local prx = (W - #pr * 8 * 1.25) / 2
			for i = 1, #pr do
				draw.char(pr:sub(i, i), prx + (i - 1) * 8 * 1.25, H / 2 + 10, 1.25, 180, 180, 200, 255)
			end
		end

		if session.state == "gameover" then
			local msg = "GAME OVER"
			local scale = 4
			local cw = 8 * scale
			local sx = (W - #msg * cw) / 2
			for i = 1, #msg do
				draw.char(msg:sub(i, i), sx + (i - 1) * cw, H / 2 - 40, scale, 255, 80, 80, 255)
			end
			local sc = "SCORE"
			local scx = (W - #sc * 8 * 2.5) / 2 - 20
			for i = 1, #sc do
				draw.char(sc:sub(i, i), scx + (i - 1) * 8 * 2.5, H / 2 + 20, 2.5, 255, 255, 255, 255)
			end
			draw.number(session.score, scx + #sc * 8 * 2.5 + 12, H / 2 + 20, 2.5, 0, 220, 255, 255)
			local pr = "PRESS R TO RESTART"
			local prx = (W - #pr * 8 * 1.5) / 2
			for i = 1, #pr do
				draw.char(pr:sub(i, i), prx + (i - 1) * 8 * 1.5, H / 2 + 70, 1.5, 160, 160, 160, 255)
			end
		end

		if session.state == "newwave" and session.wave > 0 then
			local wt = "WAVE"
			local wtx = (W - #wt * 8 * 3) / 2 - 30
			for i = 1, #wt do
				draw.char(wt:sub(i, i), wtx + (i - 1) * 8 * 3, H / 2 - 30, 3, 100, 255, 140, 255)
			end
			draw.number(session.wave, wtx + #wt * 8 * 3 + 8, H / 2 - 30, 3, 100, 255, 140, 255)
		end

		if self.showFps then
			draw.number(fps, 16, H - 22, 1.2, 80, 80, 80, 255)
		end
	end,
	keydown = function(_self, key: string)
		if key == "space" and session.state == "title" then
			startGame()
		elseif key == "r" and session.state == "gameover" then
			runtime.loadScene("scenes/default.scene.json")
		end
	end,
	onHudPlay = function(_self)
		startGame()
	end,
}
