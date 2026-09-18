// Example inspired by the ttri demo on the TIC-80 wiki:
//
//	https://tic80.com/create/learn/api/ttri
//
// BOOT paints a striped pattern into the tilesheet (the texture that ttri
// samples), then TIC renders two textured triangles forming a 64x64 quad.
// The arrow keys scale the UV window into the 128x128 tilesheet, which is
// addressed in pixels and wraps around at its edges.
//
// Build and run it with:
//
//	make ttri
//	tic80 --fs . --cmd 'new wasm & import binary build/ttri.wasm & run'
package main

import "cart/tic80"

// The tilesheet is a 128x128 4bpp image at TILES RAM (0x4000): one byte
// holds two pixels, the low nibble is the left (even x) pixel.
const (
	sheetSize = 128
)

var (
	usize int32 = 32
	vsize int32 = 32
)

// setSheetPix writes a pixel into the tilesheet; equivalent to
// tic80.Poke4(0x8000 + y*sheetSize + x, color) in nibble addressing.
// setSheetPix writes a pixel into the tilesheet.  The sheet is stored
// TILE-MAJOR: tile (x/8, y/8) occupies 32 consecutive bytes, 4 rows of 4
// bytes, two pixels per byte (low nibble = even x).
func setSheetPix(x, y int32, color uint8) {
	b := &tic80.TILES[(y>>3)*512+(x>>3)*32+(y&7)*4+(x>>1&3)]
	if x&1 == 0 {
		*b = *b&0xF0 | color
	} else {
		*b = *b&0x0F | color<<4
	}
}

//go:export BOOT
func BOOT() {
	// diagonal color bands across the whole sheet
	for y := int32(0); y < sheetSize; y++ {
		for x := int32(0); x < sheetSize; x++ {
			setSheetPix(x, y, uint8((x+y)>>3)%15+1)
		}
	}
}

func clampUV(v int32) int32 {
	if v < 8 {
		return 8
	}
	if v > sheetSize {
		return sheetSize
	}
	return v
}

//go:export TIC
func TIC() {
	tic80.Cls(1)

	if tic80.Btn(tic80.BTN_UP) {
		usize -= 2
	}
	if tic80.Btn(tic80.BTN_DOWN) {
		usize += 2
	}
	if tic80.Btn(tic80.BTN_LEFT) {
		vsize -= 2
	}
	if tic80.Btn(tic80.BTN_RIGHT) {
		vsize += 2
	}
	usize = clampUV(usize)
	vsize = clampUV(vsize)

	// two textured triangles forming a 64x64 quad; the UV window into the
	// tilesheet is scaled by usize/vsize (texture src 0 = TILES RAM)
	u := float32(usize)
	v := float32(vsize)
	tic80.TTri(0, 0, 64, 0, 0, 64, 0, 0, u, 0, 0, v, tic80.TTriOptions{})
	tic80.TTri(64, 0, 0, 64, 64, 64, u, 0, 0, v, u, v, tic80.TTriOptions{})

	tic80.Print("ARROWS SCALE UV", 4, 4, tic80.PrintOptions{Color: 15, Fixed: true})
}
