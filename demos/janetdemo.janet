# title:   game title
# author:  game developer, email, etc.
# desc:    short description
# site:    website link
# license: MIT License (change this to your license of choice)
# version: 0.1
# script: janet
# strict:  true

# Unlike other languages, the tic80 API
# is provided as a module.
(import tic80)

(var t 0)
(var x 96)
(var y 24)

(defn TIC []
  (when (tic80/btn 0) (-- y))
  (when (tic80/btn 1) (++ y))
  (when (tic80/btn 2) (-- x))
  (when (tic80/btn 3) (++ x))
  (tic80/cls 13)
  (tic80/spr (if (> (% t 60) 30) 1 3)
          x y 14 3 0 0 2 2)
  (tic80/print "HELLO WORLD!" 84 84)
  (++ t))

# <TILES>
# 001:eccccccccc888888caaaaaaaca888888cacccccccacc0ccccacc0ccccacc0ccc
# 002:ccccceee8888cceeaaaa0cee888a0ceeccca0ccc0cca0c0c0cca0c0c0cca0c0c
# 003:eccccccccc888888caaaaaaaca888888cacccccccacccccccacc0ccccacc0ccc
# 004:ccccceee8888cceeaaaa0cee888a0ceeccca0cccccca0c0c0cca0c0c0cca0c0c
# 017:cacccccccaaaaaaacaaacaaacaaaaccccaaaaaaac8888888cc000cccecccccec
# 018:ccca00ccaaaa0ccecaaa0ceeaaaa0ceeaaaa0cee8888ccee000cceeecccceeee
# 019:cacccccccaaaaaaacaaacaaacaaaaccccaaaaaaac8888888cc000cccecccccec
# 020:ccca00ccaaaa0ccecaaa0ceeaaaa0ceeaaaa0cee8888ccee000cceeecccceeee
# </TILES>

# <WAVES>
# 000:00000000ffffffff00000000ffffffff
# 001:0123456789abcdeffedcba9876543210
# 002:0123456789abcdef0123456789abcdef
# </WAVES>

# <SFX>
# 000:000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000304000000000
# </SFX>

# <PALETTE>
# 000:1a1c2c5d275db13e53ef7d57ffcd75a7f07038b76425717929366f3b5dc941a6f673eff7f4f4f494b0c2566c86333c57
# </PALETTE>

# <TRACKS>
# 000:100000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000
# </TRACKS>
