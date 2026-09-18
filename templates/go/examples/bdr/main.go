// Example inspired by the BDR glitch demo on the TIC-80 wiki:
//
//	https://tic80.com/create/learn/api/bdr
//
// Press any button to shake the screen: BDR runs between every scan line
// and jitters the screen's X offset per line, while TIC jitters the Y
// offset once per frame.  When the shake ends both offsets are reset.
//
// Build and run it with:
//
//	make bdr
//	tic80 --fs . --cmd 'new wasm & import binary build/bdr.wasm & run'
package main

import "cart/tic80"

// TIC-80 RAM: screen offset registers (0x3FF9 = X, 0x3FFA = Y, both i8).
const (
	offsetX     = 0x3FF9
	offsetY     = 0x3FFA
	shakeFrames = 30
	shakeRadius = 4
)

var (
	shake int32
	seed  uint32 = 0x1EC4B1B7
)

// nextRand is a tiny xorshift32 PRNG: allocation-free, which matters
// because BDR runs once per scan line and TinyGo's GC never frees.
func nextRand() uint32 {
	seed ^= seed << 13
	seed ^= seed >> 17
	seed ^= seed << 5
	return seed
}

// randSym returns a random value in [-shakeRadius, shakeRadius].
func randSym() int32 {
	return int32(nextRand()%(2*shakeRadius+1)) - shakeRadius
}

//go:export BOOT
func BOOT() {
	tic80.Init()
}

//go:export TIC
func TIC() {
	// Btnp(-1) is true for any newly pressed button (like btnp() in Lua).
	if tic80.Btnp(-1) {
		shake = shakeFrames
	}
	if shake > 0 {
		// Shake the screen's Y offset once per frame; BDR shakes X per
		// line.  Negative values wrap to the i8 register, like poke in Lua.
		tic80.Poke(offsetY, randSym(), 8)
		shake--
		if shake == 0 {
			tic80.Memset(offsetX, 0, 2)
		}
	}
	tic80.Cls(12)
	tic80.Print("PRESS ANY KEY TO GLITCH!", 54, 64)
}

//go:export BDR
func BDR(row int32) {
	if shake > 0 {
		tic80.Poke(offsetX, randSym(), 8)
	}
}
