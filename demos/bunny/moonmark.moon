-- title: Bunnymark in MoonScript
-- author: Rob Loach
-- desc: Benchmarking tool to see how many bunnies can fly around the screen, using MoonScript.
-- license: MIT License 
-- input: gamepad
-- script: moon
-- version: 1.1.0

screenWidth = 240
screenHeight = 136
toolbarHeight = 6
t =  0

class Bunny
  @width: 26
  @height: 32

  new: =>
    @x = math.random(0, screenWidth - @@width)
    @y = math.random(0, screenHeight - @@height)
    @speedX = math.random(-100, 100) / 60
    @speedY = math.random(-100, 100) / 60
    @sprite = 1

  draw: () =>
    spr(@sprite, @x, @y, 1, 1, 0, 0, 4, 4)

  update: () =>
    @x += @speedX
    @y += @speedY

    if @x + @@width > screenWidth
      @x = screenWidth - @@width
      @speedX *= -1
    if @x < 0
      @x = 0
      @speedX *= -1
    if @y + @@height > screenHeight
      @y = screenHeight - @@height
      @speedY *= -1
    if @y < toolbarHeight
      @y = toolbarHeight
      @speedY *= -1

class FPS
  new: =>
    @value = 0
    @frames = 0
    @lastTime = 0

  getValue: () =>
    if time() - @lastTime <= 1000
      @frames += 1
    else
      @value = @frames
      @frames = 0
      @lastTime = time()
    return @value

fps = FPS!
bunnies = {}
table.insert(bunnies, Bunny!)

export TIC=->
  -- music
  if t == 0 then
    music(0)
  if t == 6*64*2.375 then
    music(1)
  t = t + 1

  -- Input
  if btn(0)
    for i = 1, 5
      table.insert(bunnies, Bunny!)
  elseif btn(1)
    for i = 1, 5
      table.remove(bunnies, 1)

  -- Update
  for i, item in pairs bunnies
    item\update!

  -- Draw
  cls(15)
  for i, item in pairs bunnies
    item\draw!

  rect(0, 0, screenWidth, toolbarHeight, 0)
  print("Bunnies: " .. #bunnies, 1, 0, 11, false, 1, false)
  print("FPS: " .. fps\getValue!, screenWidth / 2, 0, 11, false, 1, false)

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
