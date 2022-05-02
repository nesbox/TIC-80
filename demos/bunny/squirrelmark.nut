// title: Bunnymark in Squirrel
// author: Rob Loach
// desc: Benchmarking tool to see how many bunnies can fly around the screen, using Squirrel.
// license: MIT License 
// input: gamepad
// script: squirrel
// version: 1.1.0

local ScreenWidth = 240;
local ScreenHeight = 136;
local ToolbarHeight = 6;
local t = 0;

function rndfloat(max) {
    local roll = 1.0 * max * rand() / RAND_MAX;
    return roll;
}

function rndint(max) {
    local roll = 1.0 * max * rand() / RAND_MAX;
    return roll.tointeger();
}

class Bunny {
	static width = 26;
	static height = 32;

	x = 0;
	y = 0;
	speedX = 0;
	speedY = 0;
	sprite = 1;

	constructor() {
		x = rndint(ScreenWidth - 26);
		y = rndint(ScreenHeight - 32);
		speedX = (rndfloat(200) - 100) / 60;
		speedY = (rndfloat(200) - 100) / 60;
	}

	function draw() {
		spr(sprite, x, y, 1, 1, 0, 0, 4, 4);
	}

	function update() {
		x = x + speedX;
		y = y + speedY;

		if (x + width > ScreenWidth) {
			x = ScreenWidth - width;
			speedX = speedX * - 1;
		}
		if (x < 0) {
			x = 0;
			speedX = speedX * -1;
		}
		if (y + height > ScreenHeight) {
			y = ScreenHeight - height;
			speedY = speedY * -1;
		}

		if (y < ToolbarHeight) {
			y = ToolbarHeight;
			speedY = speedY * -1;
		}
	}
}

class FPS {
	fps = 0;
	frames = 0;
	lastTime = -1000;

	function getValue() {
		if (time() - lastTime <= 1000) {
			frames = frames + 1;
		} else {
			fps = frames;
			frames = 0;
			lastTime = time();
		}
		return fps;
	}
}

local fps = FPS();
local bunnies = [Bunny()];

function TIC() {
  // Music
	if (t==0) {
	  music(0)
	}
	if (t == 6*64*2.375) {
	  music(1)
	}
	t++

	// Input
	if (btn(0)) {
		for (local i = 0; i < 5; i += 1) {
			bunnies.push(Bunny());
		}
	} else if (btn(1)) {
		for (local i = 0; i < 5; i += 1) {
			if (bunnies.len() > 0) {
				bunnies.remove(0);
			}
		}
	}

	// Update
	foreach (i,bunny in bunnies) {
		bunny.update();
	}

	// Draw
	cls(15)
	foreach (i,bunny in bunnies) {
		bunny.draw();
	}
	rect(0, 0, ScreenWidth, ToolbarHeight, 0);
	print("Bunnies: " + bunnies.len().tostring(), 1, 0, 11, false, 1, false);
	print("FPS: " + fps.getValue().tostring(), ScreenWidth / 2, 0, 11, false, 1, false);
}

// <TILES>
// 001:11111100111110dd111110dc111110dc111110dc111110dc111110dd111110dd
// 002:00011110ddd0110dccd0110dccd0110dccd0110dccd0110dcddd00dddddddddd
// 003:00001111dddd0111cccd0111cccd0111cccd0111cccd0111dcdd0111dddd0111
// 004:1111111111111111111111111111111111111111111111111111111111111111
// 017:111110dd111110dd111110dd111110dd10000ddd1eeeeddd1eeeeedd10000eed
// 018:d0ddddddd0ddddddddddddddddd0000dddddccddddddccdddddddddddddddddd
// 019:0ddd01110ddd0111dddd0111dddd0111ddddd000ddddddddddddddddddddd000
// 020:1111111111111111111111111111111101111111d0111111d011111101111111
// 033:111110ee111110ee111110ee111110ee111110ee111110ee111110ee111110ee
// 034:dddcccccddccccccddccccccddccccccddccccccdddcccccdddddddddddddddd
// 035:dddd0111cddd0111cddd0111cddd0111cddd0111dddd0111dddd0111dddd0111
// 036:1111111111111111111111111111111111111111111111111111111111111111
// 049:111110ee111110ee111110ee111110ee111110ee111110ee111110ee11111100
// 050:dddeeeeeddeeeeeed00000000111111101111111011111110111111111111111
// 051:eddd0111eedd01110eed011110ee011110ee011110ee011110ee011111001111
// 052:1111111111111111111111111111111111111111111111111111111111111111
// </TILES>

// <PALETTE>
// 000:1a1c2c5d275db13e53ef7d57ffcd75a7f07038b76425717929366f3b5dc941a6f673eff7f4f4f494b0c2566c86333c57
// </PALETTE>
