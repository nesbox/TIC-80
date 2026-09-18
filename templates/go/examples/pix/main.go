// Example inspired by the pix() examples on the TIC-80 wiki:
//
//	https://tic80.com/create/learn/api/pix
//
// Hold the A button (Z key) to paint randomly colored pixels (wiki
// example 1); release it to cycle the colors of a checkered background
// pixel by pixel (wiki example 2).
//
// Build and run it with:
//
//	make pix
//	tic80 --fs . --cmd 'new wasm & import binary build/pix.wasm & run'
package main

import "cart/tic80"

var (
	t       int32
	seed    uint32 = 0x9E3779B9
	cycling bool
)

// nextRand is a tiny xorshift32 PRNG: good enough for sparkles and
// allocation-free, which matters because TinyGo's GC never frees.
func nextRand() uint32 {
	seed ^= seed << 13
	seed ^= seed >> 17
	seed ^= seed << 5
	return seed
}

// drawBackground paints the rectangles from wiki example 2.
func drawBackground() {
	tic80.Cls(0)
	for i := int32(0); i <= 15; i++ {
		tic80.Rect(9*i, 6*i, 6*i, 3*i, uint8(i))
	}
}

//go:export BOOT
func BOOT() {
	tic80.Init()
	drawBackground()
}

//go:export TIC
func TIC() {
	if tic80.Btn(tic80.BTN_A) {
		// Wiki example 1: put a math-colored pixel at a random place,
		// 6000 times per frame.  The palette has 16 colors, so wrap the
		// hue with %16.
		for i := int32(0); i < 6000; i++ {
			x := int32(nextRand() % 240)
			y := int32(nextRand() % 136)
			seconds := int32(tic80.Time() / 1000)
			color := uint8((seconds * x * y) % 16)
			tic80.Pix(x, y, color)
		}
		cycling = false
		tic80.Print("HOLD A/Z: RANDOM PIX", 4, 4, tic80.PrintOptions{Color: 15, Fixed: true})
	} else {
		// Wiki example 2: every 13 frames, read every other pixel,
		// bump its color by one and put it back.
		if !cycling {
			cycling = true
			drawBackground()
		}
		t++
		if t > 12 {
			t = 0
			for x := int32(0); x < 240; x += 2 {
				for y := int32(0); y < 136; y += 2 {
					c := tic80.GetPix(x, y)
					tic80.Pix(x, y, uint8((c+1)%15))
				}
			}
		}
		tic80.Print("RELEASE A/Z: CYCLE PIX", 4, 4, tic80.PrintOptions{Color: 15, Fixed: true})
	}
}
