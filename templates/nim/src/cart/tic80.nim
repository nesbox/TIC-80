import internal except btn, btnp, trace
export internal except btn, btnp, trace

proc font*(text: cstring, x, y: int32, transColor, char_width, char_height: int8,
    fixed: bool, scale: int8, alt: bool): int8 {.discardable.} =
  ##  Print a string using foreground sprite data as the font.
  if transColor >= 0:
    let trans_colors = transColor.uint8
    internal.font(text, x, y, addr trans_colors, 1, char_width, char_height, fixed,
      scale, alt)
  else:
    internal.font(text, x, y, nil, 0, char_width, char_height, fixed, scale, alt)

proc map*(x: int32 = 0, y: int32 = 0, w: int32 = 30, h: int32 = 17,
          sx: int32 = 0, sy: int32 = 0, transColor: Color = Null, scale: int8 = 1,
          remap: int32 = -1) =
  ##  Draw a map region.
  if transColor.int8 >= 0:
    let trans_colors = transColor.uint8
    internal.map(x, y, w, h, sx, sy, addr trans_colors, 1, scale, remap)
  else:
    internal.map(x, y, w, h, sx, sy, nil, 0, scale, remap)

proc spr*(id, x, y: int32, transColor: Color = Null, scale: int32 = 1,
    flip: int32 = 0, rotate: int32 = 0, w: int32 = 1, h: int32 = 1) =
  ##  Draw a sprite or composite sprite.
  if transColor.int8 >= 0:
    let trans_colors = transColor.uint8
    internal.spr(id, x, y, addr trans_colors, 1, scale, flip, rotate, w, h)
  else:
    internal.spr(id, x, y, nil, 0, scale, flip, rotate, w, h)

proc ttri*(x1, y1, x2, y2, x3, y3, u1, v1, u2, v2, u3, v3: cfloat,
    texsrc: int32 = 0, transColor: Color = Null,
    z1: cfloat = 0, z2: cfloat = 0, z3: cfloat = 0, depth = false) =
  ##  Draw a triangle filled with texture.
  if transColor.int8 >= 0:
    let trans_colors = transColor.uint8
    internal.ttri(x1, y1, x2, y2, x3, y3, u1, v1, u2, v2, u3, v3, texsrc,
      addr trans_colors, 1, z1, z2, z3, depth)
  else:
    internal.ttri(x1, y1, x2, y2, x3, y3, u1, v1, u2, v2, u3, v3, texsrc,
      nil, 0, z1, z2, z3, depth)

proc btn*(index: Button): bool {.inline.} =
  ##  Get gamepad button state in current frame.
  internal.btn(index) > 0

proc btnp*(index: Button, hold: int32 = -1, period: int32 = -1): bool {.inline.} =
  ##  Get gamepad button state according to previous frame.
  internal.btnp(index, hold, period) > 0

proc mouse*(): MousePos {.inline.} = internal.mouse(addr result)
  ##  Get XY and press state of mouse/touch.

proc pmemset*(address: int32; value: int64) = pmem(address, value)
  ##  Update the persistent memory.
proc pmemget*(address: int32): uint32 = pmem(address, -1)
  ##  Access the persistent memory.

proc pixset*(x, y: int32; color: Color) = pix(x, y, color)
  ##  Set the color of a single pixel.
proc pixget*(x, y: int32): uint8 = pix(x, y, Null)
  ##  Get the color of a single pixel.

proc trace*(items: varargs[cstring, cstring]) =
  ##  Print a string to the Console.
  for item in items: internal.trace(item)

proc print*(text: string; x, y: int32; color: Color = Color15; fixed = false;
           scale: int32 = 1; alt = false): int32 {.discardable.} =
  ##  Print a string using the system font.
  print(cstring(text), x, y, color, fixed, scale, alt)

proc NimMain() {.importc.}

proc BOOT() {.exportWasm.} =
  NimMain()
