// title:   bunnymark in miniscript
// author:  khuxkm/minerobber
// desc:    Benchmarking tool to see how many bunnies can fly around the screen, using MiniScript
// license: MIT License
// input: gamepad
// script:  miniscript
// version: 1.1.0

screenWidth = 240
screenHeight = 136
toolbarHeight = 6
t = 0

Bunny = {}
Bunny.width = 26
Bunny.height = 32
Bunny.sprite = 1
Bunny.make = function
    ret = new self
    ret.x = floor(rnd*(screenWidth-self.width))
    ret.y = floor(rnd*(screenHeight-self.height))
    ret.speedX = floor((rnd-0.5)*100)/60
    ret.speedY = floor((rnd-0.5)*100)/60
    return ret
end function
Bunny.draw = function
    tic80.spr self.sprite, self.x, self.y, 1, 1, 0, 0, 4, 4
end function
Bunny.update = function
    self.x += self.speedX
    self.y += self.speedY
    
    if self.x+self.width > screenWidth then
        self.x = screenWidth - self.width
        self.speedX *= -1
    end if
    if self.x < 0 then
        self.x = 0
        self.speedX *= -1
    end if
    if self.y+self.height > screenHeight then
        self.y = screenHeight - self.height
        self.speedY *= -1
    end if
    if self.y < 0 then
        self.y = 0
        self.speedY *= -1
    end if
end function

FPS = {}
FPS.value = 0
FPS.frames = 0
FPS.lastTime = 0
FPS.getValue = function
    if tic80.time - self.lastTime <= 1000 then
        self.frames += 1
    else
        self.value = self.frames
        self.frames = 0
        self.lastTime = tic80.time
    end if
    return self.value
end function

fps = new FPS
bunnies = []
bunnies.push Bunny.make

TIC = function
    if t==0 then
        tic80.music 0
    else if t==6*64*2.375 then
        tic80.music 1
    end if
    outer.t += 1

    if tic80.btn(0) then
        for i in range(1, 5)
            bunnies.push Bunny.make
        end for
    end if
    if tic80.btn(1) then
        for i in range(1, 5)
            if bunnies.len > 0 then bunnies.pop
        end for
    end if
    
    for bunny in bunnies
        bunny.update
    end for
    
    tic80.cls 15
    for bunny in bunnies
        bunny.draw
    end for
    
    tic80.rect 0, 0, screenWidth, toolbarHeight, 0
    tic80.print "Bunnies: "+bunnies.len, 1, 0, 11, false, 1, false
    tic80.print "FPS: "+fps.getValue, screenWidth/2, 0, 11, false, 1, false
end function

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
