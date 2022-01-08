// title: Bunnymark in Wren
// author: Rob Loach
// desc: Benchmarking tool to see how many bunnies can fly around the screen, using Wren.
// license: MIT License 
// input: gamepad
// script: wren
// version: 1.1.0

import "random" for Random

var ScreenWidth = 240
var ScreenHeight = 136
var ToolbarHeight = 6

class Bunny {
	construct new(rand) {
		_x = rand.int(0, ScreenWidth - Bunny.width)
		_y = rand.int(0, ScreenHeight - Bunny.height)
		_speedX = rand.float(-100, 100) / 60
		_speedY = rand.float(-100, 100) / 60
		_sprite = 1
	}

	static width {
		return 26
	}
	static height {
		return 32
	}

	draw() {
		TIC.spr(_sprite, _x, _y, 1, 1, 0, 0, 4, 4)
	}

	update() {
		_x = _x + _speedX
		_y = _y + _speedY

		if (_x + Bunny.width > ScreenWidth) {
			_x = ScreenWidth - Bunny.width
			_speedX = _speedX * - 1
		}
		if (_x < 0) {
			_x = 0
			_speedX = _speedX * -1
		}
		if (_y + Bunny.height > ScreenHeight) {
			_y = ScreenHeight - Bunny.height
			_speedY = _speedY * -1
		}

		if (_y < ToolbarHeight) {
			_y = ToolbarHeight
			_speedY = _speedY * -1
		}
	}
}

class FPS {
	construct new() {
		_value = 0
		_frames = 0
		_lastTime = -1000
	}
	getValue() {
		if (TIC.time() - _lastTime <= 1000) {
			_frames = _frames + 1
		} else {
			_value = _frames
			_frames = 0
			_lastTime = TIC.time()
		}
		return _value
	}
}

class Game is TIC {
	construct new() {
		_fps = FPS.new()
		_random = Random.new()
		_bunnies = [Bunny.new(_random)]
		_t = 0
	}

	TIC() {
	  // Music
		if (_t == 0) {
		  TIC.music(0)
		}
		if (_t == 6*64*2.375) {
		  TIC.music(1)
		}
		_t = _t + 1

		// Input
		if (TIC.btn(0)) {
			for (i in 1...5) {
				_bunnies.add(Bunny.new(_random))
			}
		} else if (TIC.btn(1)) {
			for (i in 1...5) {
				if (_bunnies.count > 0) {
					_bunnies.removeAt(0)
				}
			}
		}

		// Update
		for (bunny in _bunnies) {
			bunny.update()
		}

		// Draw
		TIC.cls(15)
		for (bunny in _bunnies) {
			bunny.draw()
		}
		TIC.rect(0, 0, ScreenWidth, ToolbarHeight, 0)
		TIC.print("Bunnies: %(_bunnies.count)", 1, 0, 11, false, 1, false)
		TIC.print("FPS: %(_fps.getValue())", ScreenWidth / 2, 0, 11, false, 1, false)
	}
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
