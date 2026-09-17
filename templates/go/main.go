package main

import "cart/tic80"

type TICGuy struct {
	X, Y int32
}

var (
	t      int32
	mascot = TICGuy{X: 96, Y: 24}

	// Hoisted to a package var: creating a slice literal every frame would
	// allocate, and TinyGo's leaking GC never frees.
	transparent14 = []uint8{14}
)

//go:export BOOT
func BOOT() {
	tic80.Init()
}

//go:export TIC
func TIC() {
	if tic80.Btn(tic80.BTN_UP) {
		mascot.Y--
	}
	if tic80.Btn(tic80.BTN_DOWN) {
		mascot.Y++
	}
	if tic80.Btn(tic80.BTN_LEFT) {
		mascot.X--
	}
	if tic80.Btn(tic80.BTN_RIGHT) {
		mascot.X++
	}

	tic80.Cls(13)
	tic80.Spr(1+t%60/30*2, mascot.X, mascot.Y, tic80.SpriteOptions{
		W:           2,
		H:           2,
		Transparent: transparent14,
		Scale:       3,
	})
	tic80.Print("HELLO WORLD FROM GO!", 84, 84)

	t++
}

// Optional callbacks: BDR(row), SCN(row) and MENU(index) can be exported the
// same way as BOOT and TIC when needed.
