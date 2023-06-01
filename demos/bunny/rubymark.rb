# title: Bunnymark in Ruby
# author: Julia Nelz; based on the original work of Rob Loach
# desc: Benchmarking tool to see how many bunnies can fly around the screen, using Ruby.
# license: MIT License 
# input: gamepad
# script: ruby
# version: 0.1.0

ScreenWidth = 240.0
ScreenHeight = 136.0
ToolbarHeight = 6
$t = 0

class Bunny
	@@width = 26.0
	@@height = 32.0

	def initialize
		@x = Float(rand(ScreenWidth - @@width))
		@y = Float(rand(ScreenHeight - @@height))
		@speed_x = Float(rand(200) - 100) / 60.0
		@speed_y = Float(rand(200) - 100) / 60.0
		@sprite = 1
	end

	def draw
		spr @sprite, @x, @y, 1, 1, 0, 0, 4, 4
	end

	def update
		@x += @speed_x
		@y += @speed_y

		if @x + @@width > ScreenWidth
			@x = ScreenWidth - @@width
			@speed_x *= -1.0
		end
		if @x < 0.0
			@x = 0.0
			@speed_x *= -1.0
		end
		if @y + @@height > ScreenHeight
			@y = ScreenHeight - @@height
			@speed_y *= -1.0
		end
		if @y < ToolbarHeight
			@y = ToolbarHeight
			@speed_y *= -1.0
		end
	end
end

class FPS
	def initialize
		@value = 0
		@frames = 0
		@last_time = -1000
	end

	def get
		d = time - @last_time
		if d <= 1000
			@frames += 1
		else
			@value = @frames
			@frames = 0
			@last_time = time
		end

		@value
	end
end

$fps = FPS.new
$bunnies = []
$bunnies << Bunny.new

def TIC
	# Music
	music(0) if $t.zero?
	
	music(1) if $t == 6*64*2.375
	$t += 1

	# Input
	5.times { $bunnies << Bunny.new } if btn 0
	5.times { $bunnies.pop } if btn 1

	# Update
	$bunnies.each &:update

	# Draw
	cls 15
	$bunnies.each &:draw

	rect 0, 0, ScreenWidth, ToolbarHeight, 0
	print "Bunnies: #{$bunnies.count}", 1, 0, 11, false, 1, false
	print "FPS: #{$fps.get}", ScreenWidth / 2, 0, 11, false, 1, false
end

# <TILES>
# 001:11111100111110dd111110dc111110dc111110dc111110dc111110dd111110dd
# 002:00011110ddd0110dccd0110dccd0110dccd0110dccd0110dcddd00dddddddddd
# 003:00001111dddd0111cccd0111cccd0111cccd0111cccd0111dcdd0111dddd0111
# 004:1111111111111111111111111111111111111111111111111111111111111111
# 017:111110dd111110dd111110dd111110dd10000ddd1eeeeddd1eeeeedd10000eed
# 018:d0ddddddd0ddddddddddddddddd0000dddddccddddddccdddddddddddddddddd
# 019:0ddd01110ddd0111dddd0111dddd0111ddddd000ddddddddddddddddddddd000
# 020:1111111111111111111111111111111101111111d0111111d011111101111111
# 033:111110ee111110ee111110ee111110ee111110ee111110ee111110ee111110ee
# 034:dddcccccddccccccddccccccddccccccddccccccdddcccccdddddddddddddddd
# 035:dddd0111cddd0111cddd0111cddd0111cddd0111dddd0111dddd0111dddd0111
# 036:1111111111111111111111111111111111111111111111111111111111111111
# 049:111110ee111110ee111110ee111110ee111110ee111110ee111110ee11111100
# 050:dddeeeeeddeeeeeed00000000111111101111111011111110111111111111111
# 051:eddd0111eedd01110eed011110ee011110ee011110ee011110ee011111001111
# 052:1111111111111111111111111111111111111111111111111111111111111111
# </TILES>

# <PALETTE>
# 000:1a1c2c5d275db13e53ef7d57ffcd75a7f07038b76425717929366f3b5dc941a6f673eff7f4f4f494b0c2566c86333c57
# </PALETTE>

