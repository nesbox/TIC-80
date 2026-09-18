\ title:   Bunnymark in Forth
\ author:  luginf
\ desc:    Benchmarking tool to see how many bunnies can fly around the screen, using Forth.
\ license: MIT License
\ input:   gamepad
\ script:  forth
\ version: 1.1.0

240 CONSTANT SCREEN-W
136 CONSTANT SCREEN-H
  6 CONSTANT TOOLBAR-H
 26 CONSTANT BUNNY-W
 32 CONSTANT BUNNY-H

\ --- raw byte access via generic PEEK/POKE (bits=8) ---------------------
\ PEEK ( addr bits -- val )   POKE ( addr val bits -- )
: @u8  ( addr -- u8 )   8 PEEK ;
: !u8  ( u8 addr -- )   SWAP 8 POKE ;

\ u16 little-endian at byte address
: @u16 ( addr -- u16 )
  DUP @u8  SWAP 1+ @u8  256 * OR ;
: !u16 ( u16 addr -- )
  OVER 255 AND  OVER !u8
  SWAP 256 /    SWAP 1+ !u8 ;

\ sign-extend u8 (0-255) to signed integer
: @s8 ( addr -- n )   @u8 DUP 127 > IF 256 - THEN ;
: !s8 ( n addr -- )   !u8 ;

\ --- bunny data in TIC-80 RAM (sprites bank 1 + map, both unused here) --
\
\ TIC-80 RAM layout (relevant regions):
\   $04000  8 KB  tiles bank 0  ← bunny sprite lives here, read by SPR
\   $06000  8 KB  sprites bank 1  (second editor tab, no <SPRITES> in file)
\   $08000 32 KB  map            (no <MAP> in file)
\
\ Sprites bank 1 and map are contiguous and empty: 40832 bytes available.
\ We pack 4 flat arrays there (no dictionary ALLOT needed):
\   bx [i]  at BX-BASE  + i*2   u16 8.8-fp  (256 units/pixel)
\   by [i]  at BY-BASE  + i*2   u16 8.8-fp
\   bvx[i]  at BVX-BASE + i     s8  2.6-fp  (64 units/pixel/frame)
\   bvy[i]  at BVY-BASE + i     s8  2.6-fp
\   total: 6800 * 6 = 40800 bytes

6800 CONSTANT MAX-BUNNIES

$6000                      CONSTANT BX-BASE
BX-BASE  MAX-BUNNIES 2 * + CONSTANT BY-BASE
BY-BASE  MAX-BUNNIES 2 * + CONSTANT BVX-BASE
BVX-BASE MAX-BUNNIES +     CONSTANT BVY-BASE

: bx-addr  ( i -- addr )  2 * BX-BASE  + ;
: by-addr  ( i -- addr )  2 * BY-BASE  + ;
: bvx-addr ( i -- addr )  BVX-BASE + ;
: bvy-addr ( i -- addr )  BVY-BASE + ;

\ clamping limits in 8.8 fp
SCREEN-W BUNNY-W - 256 * CONSTANT X-MAX-FP
SCREEN-H BUNNY-H - 256 * CONSTANT Y-MAX-FP
TOOLBAR-H         256 * CONSTANT Y-MIN-FP

VARIABLE nbunnies  0 nbunnies !

\ --- LCG PRNG -----------------------------------------------------------
VARIABLE seed
: rnd-init   TIME seed ! ;
: rnd        seed @ 1664525 * 1013904223 + DUP seed ! ;
: rnd-n      ( n -- r )  rnd ABS SWAP MOD ;
: rnd-speed  ( -- s8 )   200 rnd-n 100 - 64 * 60 / ;

\ --- spawn / remove ------------------------------------------------------
: spawn-bunny ( -- )
  nbunnies @ MAX-BUNNIES < IF
    nbunnies @ >R
    SCREEN-W BUNNY-W -  rnd-n                       256 *  R@ bx-addr !u16
    SCREEN-H BUNNY-H - TOOLBAR-H - rnd-n TOOLBAR-H + 256 *  R@ by-addr !u16
    rnd-speed  R@ bvx-addr !s8
    rnd-speed  R@ bvy-addr !s8
    R> DROP
    nbunnies @ 1+ nbunnies !
  THEN
;

: remove-bunny ( -- )
  nbunnies @ 0> IF nbunnies @ 1- nbunnies ! THEN
;

\ --- per-bunny physics ---------------------------------------------------
\ flip-vx/vy: negate stored velocity for bunny i, leaving i on stack
: flip-vx ( i -- i )  DUP bvx-addr @s8 NEGATE OVER bvx-addr !s8 ;
: flip-vy ( i -- i )  DUP bvy-addr @s8 NEGATE OVER bvy-addr !s8 ;

: update-bunny ( i -- )
  >R
  \ x axis: new_x = x_fp + vx_s8 * 4  (convert 1/64 → 1/256 units)
  R@ bvx-addr @s8 4 *  R@ bx-addr @u16 +
  DUP X-MAX-FP > IF  DROP X-MAX-FP  R@ flip-vx DROP  THEN
  DUP 0<         IF  DROP 0          R@ flip-vx DROP  THEN
  R@ bx-addr !u16
  \ y axis
  R@ bvy-addr @s8 4 *  R@ by-addr @u16 +
  DUP Y-MAX-FP > IF  DROP Y-MAX-FP  R@ flip-vy DROP  THEN
  DUP Y-MIN-FP < IF  DROP Y-MIN-FP  R@ flip-vy DROP  THEN
  R@ by-addr !u16
  R> DROP
;

: draw-bunny ( i -- )
  >R
  1
  R@ bx-addr @u16 256 /
  R@ by-addr @u16 256 /
  1 1 0 0 4 4
  SPR
  R> DROP
;

\ --- FPS counter --------------------------------------------------------
VARIABLE fps-value
VARIABLE fps-frames
VARIABLE fps-last

: fps-init   0 fps-value !  0 fps-frames !  TIME fps-last ! ;

: fps-update ( -- )
  TIME fps-last @ - 1000 <= IF
    fps-frames @ 1+ fps-frames !
  ELSE
    fps-frames @ fps-value !
    0 fps-frames !
    TIME fps-last !
  THEN
;

\ --- string helpers -----------------------------------------------------
: n>str  ( n -- c-addr u )  S>D <# #S #> ;
: str-at ( c-addr u x y -- )  11 FALSE 1 FALSE PRINT DROP ;

\ --- HUD ----------------------------------------------------------------
: print-hud ( -- )
  0 0 SCREEN-W TOOLBAR-H 0 RECT
  S" Bunnies: "          1                0 str-at
  nbunnies @ n>str        55               0 str-at
  S" FPS: "              SCREEN-W 2/      0 str-at
  fps-value @ n>str       SCREEN-W 2/ 25 + 0 str-at
;

\ --- BOOT / TIC ---------------------------------------------------------
: BOOT
  rnd-init
  fps-init
  spawn-bunny
;

: TIC
  0 BTN IF 5 0 DO spawn-bunny  LOOP THEN
  1 BTN IF 5 0 DO remove-bunny LOOP THEN
  nbunnies @ 0 ?DO I update-bunny LOOP
  fps-update
  15 CLS
  nbunnies @ 0 ?DO I draw-bunny LOOP
  print-hud
;

\ <TILES>
\ 001:11111100111110dd111110dc111110dc111110dc111110dc111110dd111110dd
\ 002:00011110ddd0110dccd0110dccd0110dccd0110dccd0110dcddd00dddddddddd
\ 003:00001111dddd0111cccd0111cccd0111cccd0111cccd0111dcdd0111dddd0111
\ 004:1111111111111111111111111111111111111111111111111111111111111111
\ 017:111110dd111110dd111110dd111110dd10000ddd1eeeeddd1eeeeedd10000eed
\ 018:d0ddddddd0ddddddddddddddddd0000dddddccddddddccdddddddddddddddddd
\ 019:0ddd01110ddd0111dddd0111dddd0111ddddd000ddddddddddddddddddddd000
\ 020:1111111111111111111111111111111101111111d0111111d011111101111111
\ 033:111110ee111110ee111110ee111110ee111110ee111110ee111110ee111110ee
\ 034:dddcccccddccccccddccccccddccccccddccccccdddcccccdddddddddddddddd
\ 035:dddd0111cddd0111cddd0111cddd0111cddd0111dddd0111dddd0111dddd0111
\ 036:1111111111111111111111111111111111111111111111111111111111111111
\ 049:111110ee111110ee111110ee111110ee111110ee111110ee111110ee11111100
\ 050:dddeeeeeddeeeeeed00000000111111101111111011111110111111111111111
\ 051:eddd0111eedd01110eed011110ee011110ee011110ee011110ee011111001111
\ 052:1111111111111111111111111111111111111111111111111111111111111111
\ </TILES>

\ <PALETTE>
\ 000:1a1c2c5d275db13e53ef7d57ffcd75a7f07038b76425717929366f3b5dc941a6f673eff7f4f4f494b0c2566c86333c57
\ </PALETTE>
