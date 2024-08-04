import cart/tic80

var
  t = 1
  x = 96
  y = 24

proc TIC {.exportWasm.} =
  cls(Color13)

  if btn(P1_Up):    dec y
  if btn(P1_Down):  inc y
  if btn(P1_Left):  dec x
  if btn(P1_Right): inc x

  spr(1 + t mod 60 div 30 * 2, x, y, transColor=Color14, scale=3, w=2, h=2)
  print("Hello from Nim!", 84, 84)
