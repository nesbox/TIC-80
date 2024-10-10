## title: Bunnymark in Python
## author: Bryce Carson
## desc: Benchmarking tool to see how many bunnies can fly around the screen, using R
## input: gamepad
## script: r
## version: 0.0.1

screenWidth <- 240
screenHeight <- 136
toolbarHeight <- 6
t <- 0

new_bunny <- function() {
  velocityRUnif <- \() runif(1, -100.0, 100.0) / 60.0
  xV <- velocityRUnif()
  yV <- velocityRUnif()

  newBunny <-
    structure(sqrt(xV^2 + yV^2),
              width   = 26,
              height  = 32,
              x       = sample(0:(screenWidth - width), 1),
              y       = sample(0:(screenHeight - height), 1),
              speed_x = xV,
              speed_y = yV,
              sprite  = 1,
              class = "Bunny"
              )
  newBunny
}

draw_bunny <- function(bunny) {
  ## stopifnot(is(bunny, "Bunny"))
  with(attributes(bunny),
       t80.spr(sprite, x, y, 1, 1, 0, 0, 4, 4))
}

update_bunny <- function(bunny) {
  ## stopifnot(is(bunny, "Bunny"))
  attr(bunny, "x") <- attr(bunny, "x") + attr(bunny, "speed_x")
  attr(bunny, "y") <- attr(bunny, "y") + attr(bunny, "speed_y")

  if (attr(bunny, "x") + attr(bunny, "width") > screenWidth) {
    attr(bunny, "x") <- screenWidth - attr(bunny, "width")
    attr(bunny, "speed_x") <- attr(bunny, "speed_x") * -1
  }

  if (attr(bunny, "y") + attr(bunny, "height") > screenHeight) {
    attr(bunny, "y") <- screenHeight - attr(bunny, "height")
    attr(bunny, "speed_y") <- attr(bunny, "speed_y") * -1
  }

  if (attr(bunny, "x") < 0) {
    attr(bunny, "x") <- 0
    attr(bunny, "speed_x") <- attr(bunny, "speed_x") * -1.0
  }

  if (attr(bunny, "y") < toolbarHeight) {
    attr(bunny, "y") <- toolbarHeight
    attr(bunny, "speed_y") <- attr(bunny, "speed_y") * -1.0
  }
}

## FIXME: this removes the attributes. S3 classes need to define special
## methods and generics to work with various primitive classes and generics
## like list. Consider data.frame, which is a list, but which does not lose
## its attributes when you make a list of data.frames.
bunnies <- list(new_bunny())

## <TILES>
## 001:11111100111110dd111110dc111110dc111110dc111110dc111110dd111110dd
## 002:00011110ddd0110dccd0110dccd0110dccd0110dccd0110dcddd00dddddddddd
## 003:00001111dddd0111cccd0111cccd0111cccd0111cccd0111dcdd0111dddd0111
## 004:1111111111111111111111111111111111111111111111111111111111111111
## 017:111110dd111110dd111110dd111110dd10000ddd1eeeeddd1eeeeedd10000eed
## 018:d0ddddddd0ddddddddddddddddd0000dddddccddddddccdddddddddddddddddd
## 019:0ddd01110ddd0111dddd0111dddd0111ddddd000ddddddddddddddddddddd000
## 020:1111111111111111111111111111111101111111d0111111d011111101111111
## 033:111110ee111110ee111110ee111110ee111110ee111110ee111110ee111110ee
## 034:dddcccccddccccccddccccccddccccccddccccccdddcccccdddddddddddddddd
## 035:dddd0111cddd0111cddd0111cddd0111cddd0111dddd0111dddd0111dddd0111
## 036:1111111111111111111111111111111111111111111111111111111111111111
## 049:111110ee111110ee111110ee111110ee111110ee111110ee111110ee11111100
## 050:dddeeeeeddeeeeeed00000000111111101111111011111110111111111111111
## 051:eddd0111eedd01110eed011110ee011110ee011110ee011110ee011111001111
## 052:1111111111111111111111111111111111111111111111111111111111111111
## </TILES>

## <PALETTE>
## 000:1a1c2c5d275db13e53ef7d57ffcd75a7f07038b76425717929366f3b5dc941a6f673eff7f4f4f494b0c2566c86333c57
## </PALETTE>
