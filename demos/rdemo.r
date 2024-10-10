## title:   game title
## author:  game developer, email, etc.
## desc:    short description
## site:    website link
## license: MIT License (change this to your license of choice)
## version: 0.1
## script:  r

t <- 0
x <- 96
y <- 24

makeopfn <- function(f) function(x) f(x, 1)
inc <- makeopfn(`+`)
dec <- makeopfn(`-`)

TIC <- \() {
  mapply(.f = \(b, o) if (btn(b)) o,
         .x = 0:3,
         .y = list(dec(y), inc(y),
                   dec(x), inc(x)))
  t80.cls(13)
  t80.spr(id = 1 + (t %% 60) / 30 * 2,
          scale = 3,
          x, y,
          colorkey = 14,
          w = 2, h = 2)
  t80.print("HELLO WORLD!", 84, 84)
  inc(t)
}

## <TILES>
## 001:eccccccccc888888caaaaaaaca888888cacccccccacc0ccccacc0ccccacc0ccc
## 002:ccccceee8888cceeaaaa0cee888a0ceeccca0ccc0cca0c0c0cca0c0c0cca0c0c
## 003:eccccccccc888888caaaaaaaca888888cacccccccacccccccacc0ccccacc0ccc
## 004:ccccceee8888cceeaaaa0cee888a0ceeccca0cccccca0c0c0cca0c0c0cca0c0c
## 017:cacccccccaaaaaaacaaacaaacaaaaccccaaaaaaac8888888cc000cccecccccec
## 018:ccca00ccaaaa0ccecaaa0ceeaaaa0ceeaaaa0cee8888ccee000cceeecccceeee
## 019:cacccccccaaaaaaacaaacaaacaaaaccccaaaaaaac8888888cc000cccecccccec
## 020:ccca00ccaaaa0ccecaaa0ceeaaaa0ceeaaaa0cee8888ccee000cceeecccceeee
## </TILES>

## <WAVES>
## 000:00000000ffffffff00000000ffffffff
## 001:0123456789abcdeffedcba9876543210
## 002:0123456789abcdef0123456789abcdef
## </WAVES>

## <SFX>
## 000:000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000304000000000
## </SFX>

## <PALETTE>
## 000:1a1c2c5d275db13e53ef7d57ffcd75a7f07038b76425717929366f3b5dc941a6f673eff7f4f4f494b0c2566c86333c57
## </PALETTE>

## <TRACKS>
## 000:100000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000
## </TRACKS>
