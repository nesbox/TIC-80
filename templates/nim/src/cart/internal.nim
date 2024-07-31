const
  TileSize* = 8
  Width* = 240
  Height* = 136
  WidthTiles* = Width div TileSize
  HeightTiles* = Height div TileSize
  Bpp* = 4   ## bits per pixel

type
  Key* = enum
    Key_Null, Key_A, Key_B, Key_C, Key_D, Key_E, Key_F, Key_G, Key_H, Key_I, Key_J,
    Key_K, Key_L, Key_M, Key_N, Key_O, Key_P, Key_Q, Key_R, Key_S, Key_T, Key_U,
    Key_V, Key_W, Key_X, Key_Y, Key_Z, Key_0, Key_1, Key_2, Key_3, Key_4, Key_5,
    Key_6, Key_7, Key_8, Key_9, Key_Minus, Key_Equals, Key_Leftbracket,
    Key_Rightbracket, Key_Backslash, Key_Semicolon, Key_Apostrophe, Key_Grave,
    Key_Comma, Key_Period, Key_Slash, Key_Space, Key_Tab, Key_Return, Key_Backspace,
    Key_Delete, Key_Insert, Key_Pageup, Key_Pagedown, Key_Home, Key_End, Key_Up,
    Key_Down, Key_Left, Key_Right, Key_Capslock, Key_Ctrl, Key_Shift, Key_Alt

  Button* = enum
    P1_Up, P1_Down, P1_Left, P1_Right, P1_A, P1_B, P1_X, P1_Y,
    P2_Up, P2_Down, P2_Left, P2_Right, P2_A, P2_B, P2_X, P2_Y,
    P3_Up, P3_Down, P3_Left, P3_Right, P3_A, P3_B, P3_X, P3_Y,
    P4_Up, P4_Down, P4_Left, P4_Right, P4_A, P4_B, P4_X, P4_Y

  Color* = enum
    Null = -1'i8, Color0, Color1, Color2, Color3, Color4, Color5, Color6, Color7,
    Color8, Color9, Color10, Color11, Color12, Color13, Color14, Color15,

  VRAM* {.bycopy.} = object
    screen*: array[Width * Height * Bpp div 8, uint8]
    palette*: array[48, uint8] ##  16 colors.
    palletteMap*: array[8, uint8] ##  16 indices.
    borderColorAndOvrTransparency*: uint8 ##  Bank 0 is border color, bank 1 is OVR transparency.
    screenOffsetX*: int8
    screenOffsetY*: int8
    mouseCursor*: int8
    blitSegment*: uint8
    reserved*: array[3, uint8]

  MousePos* {.bycopy.} = object
    x*: int16
    y*: int16
    scrollx*: int8
    scrolly*: int8
    left*: bool
    middle*: bool
    right*: bool


##  ---------------------------
##       Pointers
##  ---------------------------

const
  Framebuffer*: ptr Vram = cast[ptr Vram](0)
  Tiles*: ptr uint8 = cast[ptr uint8](0x04000)
  Sprites*: ptr uint8 = cast[ptr uint8](0x06000)
  Map*: ptr uint8 = cast[ptr uint8](0x08000)
  Gamepads*: ptr uint8 = cast[ptr uint8](0x0ff80)
  Mouse*: ptr uint8 = cast[ptr uint8](0x0ff84)
  Keyboard*: ptr uint8 = cast[ptr uint8](0x0ff88)
  SfxState*: ptr uint8 = cast[ptr uint8](0x0ff8c)
  SoundRegisters*: ptr uint8 = cast[ptr uint8](0x0ff9c)
  Waveforms*: ptr uint8 = cast[ptr uint8](0x0ffe4)
  Sfx*: ptr uint8 = cast[ptr uint8](0x100e4)
  MusicPatterns*: ptr uint8 = cast[ptr uint8](0x11164)
  MusicTracks*: ptr uint8 = cast[ptr uint8](0x13e64)
  SoundState*: ptr uint8 = cast[ptr uint8](0x13ffc)
  StereoVolume*: ptr uint8 = cast[ptr uint8](0x14000)
  PersistentMemory*: ptr uint8 = cast[ptr uint8](0x14004)
  SpriteFlags*: ptr uint8 = cast[ptr uint8](0x14404)
  SystemFont*: ptr uint8 = cast[ptr uint8](0x14604)
  WasmFreeRam*: ptr uint8 = cast[ptr uint8](0x18000)

##  160kb


##  ---------------------------
##       Constants
##  ---------------------------

const
  TilesSize*: uint32 = 0x2000
  SpritesSize*: uint32 = 0x2000
  MapSize*: uint32 = 32640
  GamepadsSize*: uint32 = 4
  MouseSize*: uint32 = 4
  KeyboardSize*: uint32 = 4
  SfxStateSize*: uint32 = 16
  SoundRegistersSize*: uint32 = 72
  WaveformsSize*: uint32 = 256
  SfxSize*: uint32 = 4224
  MusicPatternsSize*: uint32 = 11520
  MusicTracksSize*: uint32 = 408
  SoundStateSize*: uint32 = 4
  StereoVolumeSize*: uint32 = 4
  PersistentMemorySize*: uint32 = 1024
  SpriteFlagsSize*: uint32 = 512
  SystemFontSize*: uint32 = 2048
  WasmFreeRamSize*: uint32 = 163840

##  160kb


{.push importc, codegenDecl: "__attribute__((import_name(\"$2\"))) $1 $2$3".}

##  ---------------------------
##       Drawing Procedures
##  ---------------------------

proc circ*(x, y, radius: int32; color: Color)
  ##  Draw a filled circle.

proc circb*(x, y, radius: int32; color: Color)
  ##  Draw a circle border.

proc elli*(x, y, a, b: int32; color: Color)
  ##  Draw a filled ellipse.

proc ellib*(x, y, a, b: int32; color: Color)
  ##  Draw an ellipse border.

proc clip*(x, y, width, height: int32)
  ##  Set the screen clipping region.

proc cls*(color: Color = Color0)
  ##  Clear the screen.

proc font*(text: cstring; x, y: int32; trans_colors: ptr uint8; trans_count: int8,
          char_width, char_height: int8; fixed: bool; scale: int8; alt: bool): int8
  ##  Print a string using foreground sprite data as the font.

proc line*(x0, y0, x1, y1: cfloat; color: Color)
  ##  Draw a straight line.

proc map*(x, y, w, h, sx, sy: int32; trans_colors: ptr uint8; colorCount: int8;
          scale: int8; remap: int32)
  ##  Draw a map region.

proc pix*(x, y: int32; color: Color): uint8 {.discardable.}
  ##  Get or set the color of a single pixel.

proc print*(text: cstring; x, y: int32; color: Color = Color15; fixed = false;
           scale: int32 = 1; alt = false): int32 {.discardable.}
  ##  Print a string using the system font.

proc rect*(x, y, w, h: int32; color: Color)
  ##  Draw a filled rectangle.

proc rectb*(x, y, w, h: int32; color: Color)
  ##  Draw a rectangle border.

proc spr*(id, x, y: int32; trans_colors: ptr uint8; color_count: int8;
        scale, flip, rotate, w, h: int32)
  ##  Draw a sprite or composite sprite.

proc tri*(x1, y1, x2, y2, x3, y3: cfloat; color: Color)
  ##  Draw a filled triangle.

proc trib*(x1, y1, x2, y2, x3, y3: cfloat; color: Color)
  ##  Draw a triangle border.

proc ttri*(x1, y1, x2, y2, x3, y3, u1, v1, u2, v2, u3, v3: cfloat; texsrc: int32;
          trans_colors: ptr uint8; color_count: int8; z1, z2, z3: cfloat; depth: bool)
  ##  Draw a triangle filled with texture.


##  ---------------------------
##       Input Functions
##  ---------------------------

proc btn*(button: Button): uint32
  ##  Get gamepad button state in current frame.

proc btnp*(button: Button; hold: int32 = -1; period: int32 = -1): uint32
  ##  Get gamepad button state according to previous frame.

proc key*(key: Key): bool
  ##  Get keyboard button state in current frame.

proc keyp*(key: Key; hold: int32 = -1; period: int32 = -1): bool
  ##  Get keyboard button state relative to previous frame.

proc mouse*(mouse_ptr_addy: ptr MousePos)
  ##  Get XY and press state of mouse/touch.


##  ---------------------------
##       Sound Functions
##  ---------------------------

proc music*(track, frame, row: int32; loop, sustain: bool; tempo, speed: int32)
  ##  Play or stop playing music.

proc sfx*(sfx_id, note, octave, duration, channel, volume_left, volume_right,
          speed: int32)
  ##  Play or stop playing a given sound.


##  ---------------------------
##       Memory Functions
##  ---------------------------

proc pmem*(address: int32; value: int64): uint32 {.discardable.}
  ##  Access or update the persistent memory.

proc peek*(address: int32; bits: int8 = 8): int8
  ##  Read a byte from an address in RAM.

proc peek1*(address: int32): int8
  ##  Read a single bit from an address in RAM.

proc peek2*(address: int32): int8
  ##  Read two bit value from an address in RAM.

proc peek4*(address: int32): int8
  ##  Read a nibble value from an address.

proc poke*(address: int32; value: int8; bits: int8 = 8)
  ##  Write a byte value to an address in RAM.

proc poke1*(address: int32; value: int8)
  ##  Write a single bit to an address in RAM.

proc poke2*(address: int32; value: int8)
  ##  Write a two bit value to an address in RAM.

proc poke4*(address: int32; value: int8)
  ##  Write a nibble value to an address in RAM.

proc sync*(mask: int32 = 0; bank: int8 = 0; to_cart = false)
  ##  Copy banks of RAM (sprites, map, etc) to and from the cartridge.

proc vbank*(bank: int8): int8
  ##  Switch the 16kb of banked video RAM.


##  ---------------------------
##       Utility Functions
##  ---------------------------

proc fget*(sprite_index: int32; flag: int8): bool
  ##  Retrieve a sprite flag.

proc fset*(sprite_index: int32; flag: int8; value: bool): bool {.discardable.}
  ##  Update a sprite flag.

proc mget*(x, y: int32): int32
  ##  Retrieve a map tile at given coordinates.

proc mset*(x, y, value: int32)
  ##  Update a map tile at given coordinates.


##  ---------------------------
##       System Functions
##  ---------------------------

proc exit*()
  ##  Interrupt program and return to console.

proc tstamp*(): uint32
  ##  Returns the current Unix timestamp in seconds.

proc trace*(text: cstring; color: Color = Color15)
  ##  Print a string to the Console.

{.pop.}

# suffix fixes "unreachable code executed" error
proc time2*(): cfloat {.importc,
    codegenDecl: "__attribute__((import_name(\"time\"))) $1 $2$3".}
  ##  Returns how many milliseconds have passed since game started.

import std/macros

macro exportWasm*(def: untyped): untyped =
  result = def
  result[^3] = nnkPragma.newTree(
    ident("exportc"),
    nnkExprColonExpr.newTree(
      ident("codegenDecl"),
      newStrLitNode("__attribute__((export_name(\"$2\"))) $1 $2$3")
    )
  )
