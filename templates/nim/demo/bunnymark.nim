import std/[random]
import ../cart/tic80

let ToolbarHeight = 6

type
  Bunny = object
    x, y: float32
    width: int32 = 26
    height: int32 = 32
    speedX, speedY: float32
    sprite: int32
  FPS = object
    value, frames: int32
    lastTime: float32

proc getValue(f: var FPS): int32 =
  if time() - f.lastTime <= 1000:
    inc f.frames
  else:
    f.value = f.frames
    f.frames = 0
    f.lastTime = time()
  f.value

proc newBunny: Bunny =
  Bunny(
    x: rand(0 .. Width).float32,
    y: rand(0 .. Height).float32,
    width: 26,
    height: 32,
    speedX: rand(-100.0 .. 100.0) / 60,
    speedY: rand(-100.0 .. 100.0) / 60,
    sprite: 1,
  )

proc draw(b: Bunny) =
  spr(b.sprite, b.x.int32, b.y.int32, transColor=1, w=4, h=4)

proc update(b: var Bunny) =
  b.x += b.speedX
  b.y += b.speedY

  if b.x.int32 + b.width > Width:
    b.x = float32(Width - b.width)
    b.speedX *= -1
  if b.x < 0:
    b.x = 0
    b.speedX *= -1

  if b.y.int32 + b.height > Height:
    b.y = float32(Height - b.height)
    b.speedY *= -1
  if b.y < 0:
    b.y = 0
    b.speedY *= -1

randomize(tstamp().int64)

var fps: FPS
var bunnies: seq[Bunny]

proc TIC {.exportWasm.} =
  cls(13)

  if btn(P1_Up):
    for _ in 1..5:
      bunnies.add newBunny()

  if btn(P1_Down):
    for _ in 1..5:
      if bunnies.len > 0:
        bunnies.del bunnies.high

  for bunny in bunnies.mitems:
    bunny.update()

  for bunny in bunnies.mitems:
    bunny.draw()

  rect(0, 0, Width, ToolbarHeight, 0)
  print("Bunnies: " & $bunnies.len, 1, 0, 11, false, 1, false)
  print("FPS: " & $fps.getValue(), Width div 2, 0, 11, false, 1, false)

