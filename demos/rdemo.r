## title:   game title
## author:  game developer, email, etc.
## desc:    short description
## site:    website link
## license: MIT License (change this to your license of choice)
## version: 0.1
## script:  r

## pee pee poo poo

t <- 0
x <- 96
y <- 24

inc <- \(x) eval.parent(substitute(x <- x + 1))
dec <- \(x) eval.parent(substitute(x <- x - 1))

`TIC-80` <- function() {
  mapply(\(b, o) if (.External("btn", b)) eval(o, .GlobalEnv),
         0:3,
         list(quote(dec(y)), quote(inc(y)),
              quote(dec(x)), quote(inc(x))));
  .External("cls", 13);
  .External("spr",
            id = 1 + (t %% 60) / 30 * 2,
            scale = 3,
            x, y,
            colorkey = 14,
            w = 2, h = 2);
  .External("print", "HELLO WORLD!", 84, 84);
  inc(t);
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
