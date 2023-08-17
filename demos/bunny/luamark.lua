-- title: Bunnymark in Lua
-- author: Rabia Alhaffar
-- desc: Benchmarking tool to see how many bunnies can fly around the screen, using Lua.
-- license: MIT License
-- input: gamepad
-- script: lua
-- version: 1.1.0

screenWidth = 240
screenHeight = 136
toolbarHeight = 6
t = 0

function randomFloat(lower, greater)
	return (math.random() * (greater - lower)) + lower;
end

Bunny = {}

function Bunny:new()
  local width = 26
  local height = 32

  newBunny = {
    width=width,
    height=height,
    x=math.random(0, screenWidth - width),
    y=math.random(0, screenHeight - height),
	  speedX = randomFloat(-100, 100) / 60,
	  speedY = randomFloat(-100, 100) / 60,
    sprite = 1
  }

	self.__index = self
	return setmetatable(newBunny, self)
end

function Bunny:draw()
  spr(self.sprite, self.x, self.y, 1, 1, 0, 0, 4, 4)
end

function Bunny:update()
	self.x = self.x + self.speedX
	self.y = self.y + self.speedY

	if (self.x + self.width > screenWidth) then
		self.x = screenWidth - self.width
		self.speedX = self.speedX * -1
	end
	if (self.x < 0) then
		self.x = 0
		self.speedX = self.speedX * -1
	end
	if (self.y + self.height > screenHeight) then
		self.y = screenHeight - self.height
		self.speedY = self.speedY * -1
	end
	if (self.y < toolbarHeight) then
		self.y = toolbarHeight
		self.speedY = self.speedY * -1
	end
end

FPS = {}

function FPS:new(o)
	o = o or {}
	setmetatable(o, self)
	self.__index = self
	self.value = 0
	self.frames = 0
	self.lastTime = 0
	return FPS
end

function FPS:getValue()
	if (time() - self.lastTime <= 1000) then
		self.frames = self.frames + 1
	else
		self.value = self.frames
		self.frames = 0
		self.lastTime = time()
	end
	return self.value
end

fps = FPS:new()
bunnies = {}
table.insert(bunnies, Bunny:new())

function TIC()
	-- music
	if t == 0 then
		music(0)
	end
	if t == 6*64*2.375 then
		music(1)
	end
	t = t + 1

	-- Input
	if btn(0) then
		for i = 1, 5 do
			table.insert(bunnies, Bunny:new())
		end
	end
	if btn(1) then
		for i = 1, 5 do
			if next(bunnies) ~= nil then
				table.remove(bunnies, i0)
			end
		end
	end

	-- Update
	for i, item in pairs(bunnies) do
		item:update()
	end

	-- Draw
	cls(15)
	for i, item in pairs(bunnies) do
		item:draw()
	end

	rect(0, 0, screenWidth, toolbarHeight, 0)
	print("Bunnies: " .. #bunnies, 1, 0, 11, false, 1, false)
	print("FPS: " .. fps:getValue(), screenWidth / 2, 0, 11, false, 1, false)
end

-- <TILES>
-- 001:11111100111110dd111110dc111110dc111110dc111110dc111110dd111110dd
-- 002:00011110ddd0110dccd0110dccd0110dccd0110dccd0110dcddd00dddddddddd
-- 003:00001111dddd0111cccd0111cccd0111cccd0111cccd0111dcdd0111dddd0111
-- 004:1111111111111111111111111111111111111111111111111111111111111111
-- 017:111110dd111110dd111110dd111110dd10000ddd1eeeeddd1eeeeedd10000eed
-- 018:d0ddddddd0ddddddddddddddddd0000dddddccddddddccdddddddddddddddddd
-- 019:0ddd01110ddd0111dddd0111dddd0111ddddd000ddddddddddddddddddddd000
-- 020:1111111111111111111111111111111101111111d0111111d011111101111111
-- 033:111110ee111110ee111110ee111110ee111110ee111110ee111110ee111110ee
-- 034:dddcccccddccccccddccccccddccccccddccccccdddcccccdddddddddddddddd
-- 035:dddd0111cddd0111cddd0111cddd0111cddd0111dddd0111dddd0111dddd0111
-- 036:1111111111111111111111111111111111111111111111111111111111111111
-- 049:111110ee111110ee111110ee111110ee111110ee111110ee111110ee11111100
-- 050:dddeeeeeddeeeeeed00000000111111101111111011111110111111111111111
-- 051:eddd0111eedd01110eed011110ee011110ee011110ee011110ee011111001111
-- 052:1111111111111111111111111111111111111111111111111111111111111111
-- </TILES>

-- <PALETTE>
-- 000:1a1c2c5d275db13e53ef7d57ffcd75a7f07038b76425717929366f3b5dc941a6f673eff7f4f4f494b0c2566c86333c57
-- </PALETTE>
