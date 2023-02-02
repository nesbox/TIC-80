# title: Bunnymark in Janet
# author: Alec Troemel; based on the original work of Rob Loach
# desc: Benchmarking tool to see how many bunnies can fly around the screen, using Janet
# input: gamepad
# script: janet
# version: 0.1.0

(import tic80 :as t)

(def SCREEN_WIDTH 240)
(def SCREEN_HEIGHT 136)
(def TOOLBAR_HEIGHT 6)

(var t 0)
(var RNG (math/rng (t/tstamp)))

(defn random-float [lower greater]
  (+ (* (math/rng-uniform RNG)
        (- greater lower))
     lower))

(defn draw-bunny [self]
  (t/spr (self :sprite)
         (math/round (self :x))
         (math/round (self :y))
         1 1 0 0 4 4))

(defn update-bunny [self]
  (+= (self :x) (self :speed-x))
  (+= (self :y) (self :speed-y))

  (when (> (+ (self :x) (self :width)) SCREEN_WIDTH)
    (put self :x (- SCREEN_WIDTH (self :width)))
    (*= (self :speed-x) -1))

  (when (< (self :x) 0)
    (put self :x 0)
    (*= (self :speed-x) -1))

  (when (> (+ (self :y) (self :height)) SCREEN_HEIGHT)
    (put self :y (- SCREEN_HEIGHT (self :height)))
    (*= (self :speed-y) -1))

  (when (< (self :y) 0)
    (put self :y 0)
    (*= (self :speed-y) -1)))

(defn init-bunny []
  (let [width 26 height 32]
    @{:x (random-float 0 (- SCREEN_WIDTH width))
      :y (random-float 0 (- SCREEN_HEIGHT height))
      :speed-x (/ (random-float -100 100) 60)
      :speed-y (/ (random-float -100 100) 60)
      :width 26
      :height 32
      :sprite 1
      :draw draw-bunny
      :update update-bunny}))

(defn init-fps []
  @{:value 0
    :frames 0
    :last-time -1000
    :get-value (fn fps-get-value [self]
                 (if (<= (- (t/time) (self :last-time)) 1000)
                   (+= (self :frames) 1)
                   (do (put self :value (self :frames))
                       (put self :frames 0)
                       (put self :last-time (t/time))))
                 (self :value))})

(var fps (init-fps))
(var bunnies @[(init-bunny)])

(defn TIC []
  # Music
  (when (= t 0) (t/music 0))
  (when (= t (* 6 64 2.375)) (t/music 1))
  (+= t 1)

  # Input
  (cond (t/btn 0) (for i 0 5 (array/push bunnies (init-bunny)))
        (t/btn 1) (for i 0 5 (array/pop bunnies)))

  # Update
  (each bunny bunnies (:update bunny))

  # Draw
  (t/cls 15)
  (each bunny bunnies (:draw bunny))
  (t/rect 0 0 SCREEN_WIDTH TOOLBAR_HEIGHT 0)
  (t/print (string/format "Bunnies: %n" (length bunnies))
         1 0 11 false 1 false)
  (t/print (string/format "FPS: %n" (:get-value fps))
         (/ SCREEN_WIDTH 2) 0 11 false 1 false))

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
