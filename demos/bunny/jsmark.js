// title: Bunnymark in JavaScript
// author: Rob Loach
// desc: Benchmarking tool to see how many bunnies can fly around the screen, using JavaScript.
// license: MIT License 
// input: gamepad
// script: js
// version: 1.1.0

const screenWidth = 240
const screenHeight = 136
const toolbarHeight = 6
var t = 0

function GetRandomValue(min, max) {
  return Math.random() * (max - min) + min
}

function Bunny() {
	this.x = GetRandomValue(0, screenWidth - Bunny.width)
	this.y = GetRandomValue(0, screenHeight - Bunny.height)
	this.speedX = GetRandomValue(-100, 100) / 60
	this.speedY = GetRandomValue(-100, 100) / 60
	this.sprite = 1

	this.draw = function() {
		spr(this.sprite, this.x, this.y, 1, 1, 0, 0, 4, 4)
	}

	this.update = function() {
		this.x += this.speedX
		this.y += this.speedY

		if (this.x + Bunny.width > screenWidth) {
			this.x = screenWidth - Bunny.width
			this.speedX *= -1
		}
		if (this.x < 0) {
			this.x = 0
			this.speedX *= -1
		}
		if (this.y + Bunny.height > screenHeight) {
			this.y = screenHeight - Bunny.height
			this.speedY *= -1
		}

		if (this.y < toolbarHeight) {
			this.y = toolbarHeight
			this.speedY *= -1
		}
	}
}

Bunny.width = 26
Bunny.height = 32

function FPS() {
	this.value = 0
	this.frames = 0
	this.lastTime = -1000
	this.getValue = function() {
		if (time() - this.lastTime <= 1000) {
			this.frames = this.frames + 1
		}
		else {
			this.value = this.frames
			this.frames = 0
			this.lastTime = time()
		}
		return this.value
	}
}

const fps = new FPS()
const bunnies = []
bunnies.push(new Bunny())

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
		for (var i = 0; i < 5; i++) {
			bunnies.push(new Bunny())
		}
	}
	else if (btn(1)) {
		for (var i = 0; i < 5; i++) {
			bunnies.pop()
		}
	}

	// Update
	for (var i in bunnies) {
		bunnies[i].update()
	}

	// Draw
	cls(15)
	for (var i in bunnies) {
		bunnies[i].draw()
	}
	rect(0, 0, screenWidth, toolbarHeight, 0)
	print("Bunnies: " + bunnies.length, 1, 0, 11, false, 1, false)
	print("FPS: " + fps.getValue(), screenWidth / 2, 0, 11, false, 1, false)
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
