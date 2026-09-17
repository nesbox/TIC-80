// Package tic80 provides a Go (TinyGo) API for the TIC-80 fantasy console.
//
// Games are regular Go programs compiled to WebAssembly with the target.json
// file shipped with this template.  Export the callbacks you need (BOOT, TIC,
// BDR, SCN, OVR, MENU) with //go:export, e.g.:
//
//	//go:export TIC
//	func TIC() {
//		tic80.Cls(13)
//		tic80.Print("Hello from Go!", 84, 84)
//	}
//
// This package does not allocate; the GC is "leaking" (it never collects), so
// avoid per-frame allocations in your game code.
package tic80

import "unsafe"

// Screen size in pixels.
const (
	WIDTH  int32 = 240
	HEIGHT int32 = 136
)

// Sprite/tile size in pixels and the screen bits-per-pixel.
const (
	TILE_SIZE int32 = 8
	BPP       int32 = 4
)

// Gamepad button codes for player 1.  Player N (1-4) buttons start at
// 8*(N-1), e.g. player 2's A button is BTN_A + 8.
const (
	BTN_UP int32 = iota
	BTN_DOWN
	BTN_LEFT
	BTN_RIGHT
	BTN_A
	BTN_B
	BTN_X
	BTN_Y
)

// Keyboard key codes.
const (
	KEY_UNKNOWN int32 = iota
	KEY_A
	KEY_B
	KEY_C
	KEY_D
	KEY_E
	KEY_F
	KEY_G
	KEY_H
	KEY_I
	KEY_J
	KEY_K
	KEY_L
	KEY_M
	KEY_N
	KEY_O
	KEY_P
	KEY_Q
	KEY_R
	KEY_S
	KEY_T
	KEY_U
	KEY_V
	KEY_W
	KEY_X
	KEY_Y
	KEY_Z
	KEY_0
	KEY_1
	KEY_2
	KEY_3
	KEY_4
	KEY_5
	KEY_6
	KEY_7
	KEY_8
	KEY_9
	KEY_MINUS
	KEY_EQUALS
	KEY_LEFTBRACKET
	KEY_RIGHTBRACKET
	KEY_BACKSLASH
	KEY_SEMICOLON
	KEY_APOSTROPHE
	KEY_GRAVE
	KEY_COMMA
	KEY_PERIOD
	KEY_SLASH
	KEY_SPACE
	KEY_TAB
	KEY_RETURN
	KEY_BACKSPACE
	KEY_DELETE
	KEY_INSERT
	KEY_PAGEUP
	KEY_PAGEDOWN
	KEY_HOME
	KEY_END
	KEY_UP
	KEY_DOWN
	KEY_LEFT
	KEY_RIGHT
	KEY_CAPSLOCK
	KEY_CTRL
	KEY_SHIFT
	KEY_ALT
	KEY_ESCAPE
	KEY_F1
	KEY_F2
	KEY_F3
	KEY_F4
	KEY_F5
	KEY_F6
	KEY_F7
	KEY_F8
	KEY_F9
	KEY_F10
	KEY_F11
	KEY_F12
	KEY_NUMPAD0
	KEY_NUMPAD1
	KEY_NUMPAD2
	KEY_NUMPAD3
	KEY_NUMPAD4
	KEY_NUMPAD5
	KEY_NUMPAD6
	KEY_NUMPAD7
	KEY_NUMPAD8
	KEY_NUMPAD9
	KEY_NUMPADPLUS
	KEY_NUMPADMINUS
	KEY_NUMPADMULTIPLY
	KEY_NUMPADDIVIDE
	KEY_NUMPADENTER
	KEY_NUMPADPERIOD
)

// Section masks for Sync.  Pass -1 to sync all sections.
const (
	SYNC_TILES int32 = 1 << iota
	SYNC_SPRITES
	SYNC_MAP
	SYNC_SFX
	SYNC_MUSIC
	SYNC_PALETTE
	SYNC_FLAGS
	SYNC_SCREEN
)

// Flip modes for Spr.
type Flip int32

const (
	FLIP_NONE Flip = iota
	FLIP_HORIZONTAL
	FLIP_VERTICAL
	FLIP_BOTH
)

// Rotation modes for Spr.
type Rotate int32

const (
	ROTATE_NONE Rotate = iota
	ROTATE_90
	ROTATE_180
	ROTATE_270
)

// Texture sources for TTri.
type TextureSource int32

const (
	TEXTURE_TILES TextureSource = iota
	TEXTURE_MAP
	TEXTURE_VBANK1
)

// TIC-80 reserves the first 96 KiB of wasm linear memory for its RAM.  These
// point into it; access via the raw TIC-80 functions (peek/poke or the API)
// unless you know what you are doing.
var (
	// Video RAM, 4bpp: one byte holds two pixels.
	FRAMEBUFFER = (*[WIDTH * HEIGHT / 2]uint8)(unsafe.Pointer(uintptr(0x00000)))
	PALETTE     = (*[48]uint8)(unsafe.Pointer(uintptr(0x03FC0))) // 16 RGB triples
	PALETTE_MAP = (*[8]uint8)(unsafe.Pointer(uintptr(0x03FF0)))  // 16 nibbles
	// Border color (bank 0) / OVR transparency (bank 1).
	BORDER_COLOR = (*uint8)(unsafe.Pointer(uintptr(0x03FF8)))
	// X/Y screen offset, -127..127.
	SCREEN_OFFSET = (*[2]int8)(unsafe.Pointer(uintptr(0x03FF9)))
	MOUSE_CURSOR  = (*uint8)(unsafe.Pointer(uintptr(0x03FFB)))
	BLIT_SEGMENT  = (*uint8)(unsafe.Pointer(uintptr(0x03FFC)))

	TILES           = (*[8192]uint8)(unsafe.Pointer(uintptr(0x04000)))
	SPRITES         = (*[8192]uint8)(unsafe.Pointer(uintptr(0x06000)))
	MAP             = (*[32640]uint8)(unsafe.Pointer(uintptr(0x08000)))
	GAMEPADS        = (*[4]uint8)(unsafe.Pointer(uintptr(0x0FF80)))
	MOUSE           = (*[4]uint8)(unsafe.Pointer(uintptr(0x0FF84)))
	KEYBOARD        = (*[4]uint8)(unsafe.Pointer(uintptr(0x0FF88)))
	SFX_STATE       = (*[16]uint8)(unsafe.Pointer(uintptr(0x0FF8C)))
	SOUND_REGISTERS = (*[72]uint8)(unsafe.Pointer(uintptr(0x0FF9C)))
	WAVEFORMS       = (*[256]uint8)(unsafe.Pointer(uintptr(0x0FFE4)))
	SFX             = (*[4224]uint8)(unsafe.Pointer(uintptr(0x100E4)))
	MUSIC_PATTERNS  = (*[11520]uint8)(unsafe.Pointer(uintptr(0x11164)))
	MUSIC_TRACKS    = (*[408]uint8)(unsafe.Pointer(uintptr(0x13E64)))
	SOUND_STATE     = (*[4]uint8)(unsafe.Pointer(uintptr(0x13FFC)))
	STEREO_VOLUME   = (*[4]uint8)(unsafe.Pointer(uintptr(0x14000)))
	PERSISTENT_RAM  = (*[1024]uint8)(unsafe.Pointer(uintptr(0x14004)))
	SPRITE_FLAGS    = (*[512]uint8)(unsafe.Pointer(uintptr(0x14404)))
	SYSTEM_FONT     = (*[2048]uint8)(unsafe.Pointer(uintptr(0x14604)))

	// Free memory above TIC-80's RAM (160 KiB, up to the 256 KiB limit).
	WASM_FREE_RAM = (*[160 * 1024]uint8)(unsafe.Pointer(uintptr(0x18000)))
)

// Raw API:  These declarations match the TIC-80 wasm ABI one-to-one (i32/i32/i64/f32/pointers).  Prefer the wrappers below.

//go:export btn
func sysBtn(index int32) int32

//go:export btnp
func sysBtnp(index int32, hold int32, period int32) int32

//go:export key
func sysKey(id int32) int32

//go:export keyp
func sysKeyp(id int32, hold int32, period int32) int32

//go:export mouse
func sysMouse(m *Mouse)

//go:export clip
func sysClip(x int32, y int32, w int32, h int32)

//go:export cls
func sysCls(color int32)

//go:export circ
func sysCirc(x int32, y int32, radius int32, color int32)

//go:export circb
func sysCircb(x int32, y int32, radius int32, color int32)

//go:export elli
func sysElli(x int32, y int32, a int32, b int32, color int32)

//go:export ellib
func sysEllib(x int32, y int32, a int32, b int32, color int32)

//go:export line
func sysLine(x0 float32, y0 float32, x1 float32, y1 float32, color int32)

//go:export rect
func sysRect(x int32, y int32, w int32, h int32, color int32)

//go:export rectb
func sysRectb(x int32, y int32, w int32, h int32, color int32)

//go:export tri
func sysTri(x1 float32, y1 float32, x2 float32, y2 float32, x3 float32, y3 float32, color int32)

//go:export trib
func sysTrib(x1 float32, y1 float32, x2 float32, y2 float32, x3 float32, y3 float32, color int32)

//go:export ttri
func sysTtri(x1 float32, y1 float32, x2 float32, y2 float32, x3 float32, y3 float32,
	u1 float32, v1 float32, u2 float32, v2 float32, u3 float32, v3 float32,
	texsrc int32, trans *uint8, transCount int32,
	z1 float32, z2 float32, z3 float32, depth int32)

//go:export spr
func sysSpr(id int32, x int32, y int32, trans *uint8, transCount int32, scale int32, flip int32, rotate int32, w int32, h int32)

//go:export map
func sysMap(x int32, y int32, w int32, h int32, sx int32, sy int32, trans *uint8, transCount int32, scale int32, remap *mapData)

//go:export mget
func sysMget(x int32, y int32) int32

//go:export mset
func sysMset(x int32, y int32, tileID int32)

//go:export pix
func sysPix(x int32, y int32, color int32) int32

//go:export print
func sysPrint(text *uint8, x int32, y int32, color int32, fixed int32, scale int32, smallFont int32) int32

//go:export font
func sysFont(text *uint8, x int32, y int32, trans *uint8, transCount int32, charWidth int32, charHeight int32, fixed int32, scale int32, alt int32) int32

//go:export trace
func sysTrace(text *uint8, color int32)

//go:export music
func sysMusic(track int32, frame int32, row int32, loop int32, sustain int32, tempo int32, speed int32)

//go:export sfx
func sysSfx(id int32, note int32, octave int32, duration int32, channel int32, volumeLeft int32, volumeRight int32, speed int32)

//go:export pmem
func sysPmem(index int32, value int64) int32

//go:export peek
func sysPeek(address int32, bits int32) int32

//go:export peek1
func sysPeek1(address int32) int32

//go:export peek2
func sysPeek2(address int32) int32

//go:export peek4
func sysPeek4(address int32) int32

//go:export poke
func sysPoke(address int32, value int32, bits int32)

//go:export poke1
func sysPoke1(address int32, value int32)

//go:export poke2
func sysPoke2(address int32, value int32)

//go:export poke4
func sysPoke4(address int32, value int32)

//go:export sync
func sysSync(mask int32, bank int32, toCart int32)

//go:export vbank
func sysVbank(bank int32) int32

//go:export time
func sysTime() float32

//go:export tstamp
func sysTstamp() int32

//go:export exit
func sysExit()

// Text helpers
// TIC-80 wants NUL-terminated strings, which Go strings don't provide.
//  These copy into a shared static buffer: no allocations, safe because TIC-80 consumes the text before returning.

var textBuf [256]byte

func putText(s string) *uint8 {
	n := copy(textBuf[:len(textBuf)-1], s)
	textBuf[n] = 0
	return &textBuf[0]
}

// Input                                                                     │

// Mouse reports the state of the mouse/touch input.
type Mouse struct {
	X       int16
	Y       int16
	ScrollX int8
	ScrollY int8
	Left    bool
	Middle  bool
	Right   bool
}

// Btn returns true while the given button (BTN_UP..BTN_Y) is held.  Pass -1
// to get the bitmask of all held buttons instead.
func Btn(id int32) bool {
	return sysBtn(id) != 0
}

// BtnBits returns the bitmask of all held buttons for all gamepads.
func BtnBits() int32 {
	return sysBtn(-1)
}

// Btnp returns true once when the button is pressed, repeating while held
// according to the TIC-80 defaults.
func Btnp(id int32) bool {
	return sysBtnp(id, -1, -1) != 0
}

// BtnpWith returns true when the button is pressed, waiting hold frames for
// the first repeat and every period frames after that.
func BtnpWith(id int32, hold int32, period int32) bool {
	return sysBtnp(id, hold, period) != 0
}

// Key returns true while the given key is held.
func Key(id int32) bool {
	return sysKey(id) != 0
}

// Keyp returns true once when the key is pressed, repeating while held
// according to the TIC-80 defaults.
func Keyp(id int32) bool {
	return sysKeyp(id, -1, -1) != 0
}

// KeypWith returns true when the key is pressed, waiting hold frames for the
// first repeat and every period frames after that.
func KeypWith(id int32, hold int32, period int32) bool {
	return sysKeyp(id, hold, period) != 0
}

// GetMouse returns the current mouse/touch state.
func GetMouse() (m Mouse) {
	sysMouse(&m)
	return
}

//  Graphics

// Clip sets the drawing clip region; Clip(0, 0, 0, 0) resets it to the whole
// screen.
func Clip(x int32, y int32, w int32, h int32) {
	sysClip(x, y, w, h)
}

// NoClip resets the clip region to the whole screen.
func NoClip() {
	sysClip(0, 0, 0, 0)
}

// Cls clears the screen with the given color.
func Cls(color uint8) {
	sysCls(int32(color))
}

// Circ draws a filled circle.
func Circ(x int32, y int32, radius int32, color uint8) {
	sysCirc(x, y, radius, int32(color))
}

// Circb draws a circle border.
func Circb(x int32, y int32, radius int32, color uint8) {
	sysCircb(x, y, radius, int32(color))
}

// Elli draws a filled ellipse.
func Elli(x int32, y int32, a int32, b int32, color uint8) {
	sysElli(x, y, a, b, int32(color))
}

// Ellib draws an ellipse border.
func Ellib(x int32, y int32, a int32, b int32, color uint8) {
	sysEllib(x, y, a, b, int32(color))
}

// Line draws a straight line.
func Line(x0 float32, y0 float32, x1 float32, y1 float32, color uint8) {
	sysLine(x0, y0, x1, y1, int32(color))
}

// Rect draws a filled rectangle.
func Rect(x int32, y int32, w int32, h int32, color uint8) {
	sysRect(x, y, w, h, int32(color))
}

// Rectb draws a rectangle border.
func Rectb(x int32, y int32, w int32, h int32, color uint8) {
	sysRectb(x, y, w, h, int32(color))
}

// Tri draws a filled triangle.
func Tri(x1 float32, y1 float32, x2 float32, y2 float32, x3 float32, y3 float32, color uint8) {
	sysTri(x1, y1, x2, y2, x3, y3, int32(color))
}

// Trib draws a triangle border.
func Trib(x1 float32, y1 float32, x2 float32, y2 float32, x3 float32, y3 float32, color uint8) {
	sysTrib(x1, y1, x2, y2, x3, y3, int32(color))
}

// TTriOptions holds the optional parameters of TTri.
type TTriOptions struct {
	TextureSource TextureSource
	Transparent   []uint8
	Z1, Z2, Z3    float32
	Depth         bool
}

// TTri draws a triangle filled with a texture taken from tiles, the map or vbank 1.
func TTri(x1, y1, x2, y2, x3, y3, u1, v1, u2, v2, u3, v3 float32, opts ...TTriOptions) {
	o := TTriOptions{}
	if len(opts) > 0 {
		o = opts[0]
	}
	trans, count := transPtr(o.Transparent)
	sysTtri(x1, y1, x2, y2, x3, y3, u1, v1, u2, v2, u3, v3,
		int32(o.TextureSource), trans, count,
		o.Z1, o.Z2, o.Z3, boolToInt(o.Depth))
}

// SpriteOptions holds the optional parameters of Spr.
type SpriteOptions struct {
	Transparent []uint8
	Scale       int32
	Flip        Flip
	Rotate      Rotate
	W, H        int32
}

// Spr draws a sprite (or a W x H composite) with the default options.
func Spr(id int32, x int32, y int32, opts ...SpriteOptions) {
	o := SpriteOptions{Scale: 1, W: 1, H: 1}
	if len(opts) > 0 {
		o = opts[0]
		o.Scale = maxI32(o.Scale, 1)
		o.W = maxI32(o.W, 1)
		o.H = maxI32(o.H, 1)
	}
	trans, count := transPtr(o.Transparent)
	sysSpr(id, x, y, trans, count,
		o.Scale, int32(o.Flip), int32(o.Rotate), o.W, o.H)
}

// RemapResult is passed to a map remap callback; modify it in place to
// substitute the tile that gets drawn at that cell.  The default values are
// the tile originally stored in the map.
type RemapResult struct {
	Index  uint8
	_      [3]byte
	Flip   int32
	Rotate int32
}

// MapOptions holds the optional parameters of Map.  Remap, if set, is called
// for every cell drawn.
type MapOptions struct {
	X, Y, W, H  int32
	SX, SY      int32
	Transparent []uint8
	Scale       int32
	Remap       func(x, y int32, res *RemapResult)
}

// Map draws a map region with the default options.
func Map(opts ...MapOptions) {
	o := MapOptions{X: 0, Y: 0, W: 30, H: 17, SX: 0, SY: 0, Scale: 1}
	if len(opts) > 0 {
		o = opts[0]
		o.Scale = maxI32(o.Scale, 1)
	}
	trans, count := transPtr(o.Transparent)
	sysMap(o.X, o.Y, o.W, o.H, o.SX, o.SY, trans, count, o.Scale, prepareRemap(o.Remap))
}

// Mget returns the map tile at the given coordinates.
func Mget(x int32, y int32) int32 {
	return sysMget(x, y)
}

// Mset sets the map tile at the given coordinates.
func Mset(x int32, y int32, tileID int32) {
	sysMset(x, y, tileID)
}

// Pix draws a single pixel in the given color.
func Pix(x int32, y int32, color uint8) {
	sysPix(x, y, int32(color))
}

// GetPix returns the color of the pixel at the given coordinates.
func GetPix(x int32, y int32) int32 {
	// The runtime only fills the low byte of the return slot for this
	// import, so mask off the garbage above it.
	return sysPix(x, y, -1) & 0xFF
}

// Text output

// PrintOptions holds the optional parameters of Print and Font.
type PrintOptions struct {
	Color     uint8
	Fixed     bool
	Scale     int32
	SmallFont bool
}

// Print prints a string with the system font and returns the text width.
func Print(text string, x int32, y int32, opts ...PrintOptions) int32 {
	o := PrintOptions{Color: 15, Scale: 1}
	if len(opts) > 0 {
		o = opts[0]
		o.Scale = maxI32(o.Scale, 1)
	}
	return sysPrint(putText(text), x, y, int32(o.Color),
		boolToInt(o.Fixed), o.Scale, boolToInt(o.SmallFont))
}

// FontOptions holds the optional parameters of Font.
type FontOptions struct {
	Transparent []uint8
	CharWidth   int32
	CharHeight  int32
	Fixed       bool
	Scale       int32
	AltFont     bool
}

// Font prints a string using foreground sprite data as the font and returns
// the text width.
func Font(text string, x int32, y int32, opts ...FontOptions) int32 {
	o := FontOptions{CharWidth: 8, CharHeight: 8, Scale: 1}
	if len(opts) > 0 {
		o = opts[0]
		o.Scale = maxI32(o.Scale, 1)
	}
	trans, count := transPtr(o.Transparent)
	return sysFont(putText(text), x, y, trans, count,
		o.CharWidth, o.CharHeight, boolToInt(o.Fixed), o.Scale, boolToInt(o.AltFont))
}

// Trace prints a message to the TIC-80 console.
func Trace(text string, color uint8) {
	sysTrace(putText(text), int32(color))
}

// Sound

// MusicOptions holds the optional parameters of Music.
type MusicOptions struct {
	Frame   int32
	Row     int32
	Loop    bool
	Sustain bool
	Tempo   int32
	Speed   int32
}

// Music plays (or, with a negative track, stops) music.
func Music(track int32, opts ...MusicOptions) {
	o := MusicOptions{Frame: -1, Row: -1, Loop: true, Tempo: -1, Speed: -1}
	if len(opts) > 0 {
		o = opts[0]
	}
	sysMusic(track, o.Frame, o.Row, boolToInt(o.Loop), boolToInt(o.Sustain), o.Tempo, o.Speed)
}

// SfxOptions holds the optional parameters of Sfx.
type SfxOptions struct {
	Note        int32
	Octave      int32
	Duration    int32
	Channel     int32
	VolumeLeft  int32
	VolumeRight int32
	Speed       int32
}

// Sfx plays a sound effect.
func Sfx(id int32, opts ...SfxOptions) {
	o := SfxOptions{Note: -1, Octave: -1, Duration: -1, VolumeLeft: 15, VolumeRight: 15}
	if len(opts) > 0 {
		o = opts[0]
	}
	sysSfx(id, o.Note, o.Octave, o.Duration, o.Channel, o.VolumeLeft, o.VolumeRight, o.Speed)
}

// Memory access

// PmemSet writes a 32-bit value into persistent memory.
func PmemSet(index int32, value int32) {
	sysPmem(index, int64(value))
}

// PmemGet reads a 32-bit value from persistent memory.
func PmemGet(index int32) int32 {
	return sysPmem(index, -1)
}

// Peek reads a byte from RAM (bits: 1, 2, 4 or 8).
func Peek(address int32, bits int32) int32 {
	return sysPeek(address, bits) & 0xFF
}

// PeekByte reads an 8-bit value from RAM.
func PeekByte(address int32) int32 {
	return sysPeek(address, 8) & 0xFF
}

// Peek4 reads a 4-bit value from RAM.
func Peek4(address int32) int32 {
	return sysPeek4(address) & 0xF
}

// Peek2 reads a 2-bit value from RAM.
func Peek2(address int32) int32 {
	return sysPeek2(address) & 0x3
}

// Peek1 reads a single bit from RAM.
func Peek1(address int32) int32 {
	return sysPeek1(address) & 0x1
}

// Poke writes a byte to RAM (bits: 1, 2, 4 or 8).
func Poke(address int32, value int32, bits int32) {
	sysPoke(address, value, bits)
}

// PokeByte writes an 8-bit value to RAM.
func PokeByte(address int32, value int32) {
	sysPoke(address, value, 8)
}

// Poke4 writes a 4-bit value to RAM.
func Poke4(address int32, value int32) {
	sysPoke4(address, value)
}

// Poke2 writes a 2-bit value to RAM.
func Poke2(address int32, value int32) {
	sysPoke2(address, value)
}

// Poke1 writes a single bit to RAM.
func Poke1(address int32, value int32) {
	sysPoke1(address, value)
}

// Sync copies banks of RAM (sprites, map, etc.) to and from the cartridge.
// mask is a combination of the SYNC_* constants (or -1 for all), bank is
// 0-7 (or -1 for 0) and toCart selects the direction.
func Sync(mask int32, bank int32, toCart bool) {
	sysSync(mask, bank, boolToInt(toCart))
}

// Memset fills length bytes of TIC-80's RAM starting at address with value.
// TIC-80's RAM lives at the start of wasm linear memory, so this writes
// memory directly (and the compiler lowers the loop to memory.fill).  The
// memset/memcpy import names can't be declared in TinyGo: they are reserved
// libcall names there.
func Memset(address int32, value int32, length int32) {
	if length <= 0 {
		return
	}
	mem := unsafe.Slice((*uint8)(unsafe.Pointer(uintptr(address))), int(length))
	for i := range mem {
		mem[i] = uint8(value)
	}
}

// Memcpy copies length bytes of TIC-80's RAM from src to dest (lowered to
// memory.copy by the compiler).
func Memcpy(dest int32, src int32, length int32) {
	if length <= 0 {
		return
	}
	srcMem := unsafe.Slice((*uint8)(unsafe.Pointer(uintptr(src))), int(length))
	dstMem := unsafe.Slice((*uint8)(unsafe.Pointer(uintptr(dest))), int(length))
	copy(dstMem, srcMem)
}

// Vbank switches between the two 16 KiB video banks and returns the
// previously active one.
func Vbank(bank int32) int32 {
	return sysVbank(bank) & 0xFF
}

// System

// Time returns the milliseconds passed since the game started.
func Time() float32 {
	return sysTime()
}

// Tstamp returns the current Unix timestamp in seconds.
func Tstamp() int32 {
	return sysTstamp()
}

// Exit interrupts the program and returns to the console.
func Exit() {
	sysExit()
}

// Internals

// zeroTrans is an empty (one entry, zero) transparent color list: TIC-80
// reads trans colors through a pointer and this keeps the wrapper API
// allocation-free.
var zeroTrans [1]uint8

func transPtr(colors []uint8) (*uint8, int32) {
	if len(colors) == 0 {
		return &zeroTrans[0], 0
	}
	return &colors[0], int32(len(colors))
}

func boolToInt(b bool) int32 {
	if b {
		return 1
	}
	return 0
}

func maxI32(a int32, b int32) int32 {
	if a < b {
		return b
	}
	return a
}

// The map remap callback is a low-level wasm mechanism: TIC-80 looks the
// callback up in the module's function table (a wasm function pointer is a
// table index) and calls it with (user data, x, y, pointer to a
// RemapResult), expecting the result to be updated in place.
//
// mapData mirrors the MapData struct the runtime reads from linear memory.
type mapData struct {
	FuncIndex int32
	UserData  uint32
	ResPtr    uint32
}

var (
	activeRemap   func(x, y int32, res *RemapResult)
	remapResult   RemapResult
	remapData     mapData
	remapOpaque   int32
	remapTrampVal = remapTrampoline
)

// remapTrampoline is the function whose table index is handed to TIC-80.  It
// forwards to the user callback currently installed in activeRemap.
func remapTrampoline(data uint32, x int32, y int32, res *RemapResult) {
	if activeRemap != nil {
		activeRemap(x, y, res)
	}
}

// remapTrampolineAlt is never called at runtime.  It only exists so the
// compiler cannot constant-fold the func value below into a single known
// function: folding it would remove the trampoline's function-table entry,
// which TIC-80 needs to call the callback back.
func remapTrampolineAlt(data uint32, x int32, y int32, res *RemapResult) {
	if activeRemap != nil {
		remapOpaque++
		activeRemap(x, y, res)
	}
}

func prepareRemap(remap func(x, y int32, res *RemapResult)) *mapData {
	if remap == nil {
		return nil
	}
	activeRemap = nil
	// Reaching the trampoline through an opaque branch and an indirect call
	// keeps it address-taken: that is what makes wasm-ld put it into the
	// function table and resolve its table index.  The call itself is a
	// no-op because activeRemap is not installed yet.
	if Peek(int32(uintptr(unsafe.Pointer(&remapOpaque))), 8) == 0x7F {
		remapTrampVal = remapTrampolineAlt
	}
	remapTrampVal(0, 0, 0, &remapResult)
	activeRemap = remap
	remapData.FuncIndex = int32(*(*uintptr)(unsafe.Add(unsafe.Pointer(&remapTrampVal), 4)))
	remapData.UserData = 0
	remapData.ResPtr = uint32(uintptr(unsafe.Pointer(&remapResult)))
	return &remapData
}

// Init runs the TinyGo runtime initialization: heap limits, GC state and
// package initializers.  The TIC-80 wasm runtime never calls the module's
// _initialize entry point, so without Init the heap is never set up and
// the first allocation terminates the game ("[trap] unreachable executed").
//
// Call Init() first thing in BOOT, before anything that allocates:
//
//	//go:export BOOT
//	func BOOT() {
//		tic80.Init()
//		// ...
//	}
//
// Programs that never allocate are fine without it, but call it anyway.
func Init() {
	if initialized {
		return
	}
	initialized = true
	wasmEntryReactor()
}

var initialized bool

//go:linkname wasmEntryReactor runtime.wasmEntryReactor
func wasmEntryReactor()

// TinyGo requires a main function; this links it into package main so user
// code doesn't have to define one.
//
//go:linkname main main.main
func main() {}
