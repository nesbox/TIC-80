// Example inspired by the BDR palette gradient on the TIC-80 wiki:
//
//	https://tic80.com/create/learn/api/bdr
//
// BDR reprograms the palette between every scan line, which is how TIC-80
// can show more than 16 colors at once: red varies with row/12, green with
// row%12 and blue with the color index, producing a smooth gradient over
// vertical bars drawn on both video banks.
//
// Build and run it with:
//
//	make bdr2
//	tic80 --fs . --cmd 'new wasm & import binary build/bdr2.wasm & run'
package main

import "cart/tic80"

// setColor writes an RGB triple into the palette of the ACTIVE vram bank.
// tic80.Vbank swaps the 16 KiB vram block in linear memory, so writing
// through tic80.PALETTE always targets the active bank (this is what poke
// does in the Lua original, minus the host call per byte).
func setColor(index int32, r, g, b uint8) {
	i := index * 3
	tic80.PALETTE[i+0] = r
	tic80.PALETTE[i+1] = g
	tic80.PALETTE[i+2] = b
}

//go:export BOOT
func BOOT() {
	tic80.Init()
}

//go:export TIC
func TIC() {
	// Draw vertical bars (colors 1-15) on both video banks.
	for j := int32(0); j <= 1; j++ {
		tic80.Vbank(j)
		for i := int32(0); i <= 14; i++ {
			tic80.Rect(i*16+8*j, 0, 8, 136, uint8(i+1))
		}
	}
}

//go:export BDR
func BDR(row int32) {
	red := uint8((row / 12) * 21)
	green := uint8((row % 12) * 21)
	for j := int32(0); j <= 1; j++ {
		tic80.Vbank(j)
		for i := int32(0); i < 16; i++ {
			blue := uint8(i*8 + j*128)
			setColor(i, red, green, blue)
		}
	}
}
