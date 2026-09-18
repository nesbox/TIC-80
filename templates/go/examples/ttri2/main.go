// Example inspired by the sprite rotation demo from the TIC-80 wiki:
//
//	ttri is called twice (one quad) with per-vertex z values, which turns on
//	perspective-correct texture mapping - that is what makes the sprite look
//	like a 3D card rotating around the X/Y/Z axes.
//
// The texture is the 2x2-tile sprite at UV (8,0)-(24,16) - with the demo
// cartridge (cart.wasmp) that is the TIC guy.  Color 14 is the chroma key.
//
// Arrow keys: UP rotates around X, DOWN around Y, LEFT around Z and RIGHT
// shifts the rotation center through the UV window (the warp trick).
//
// Build and run it with:
//
//	make ttri2
//	tic80 --fs . --cmd 'load cart.wasmp & import binary build/ttri2.wasm & save game.tic & run'
package main

import (
	"cart/tic80"
	"math"
)

// The sprite lives in the tilesheet at UV (8,0)-(24,16); color 14 is the
// chroma key (transparent).
const (
	u1, v1 = float32(8), float32(0)
	u2, v2 = float32(24), float32(0)
	u3, v3 = float32(24), float32(16)
	u4, v4 = float32(8), float32(16)

	zOffset = float32(300) // keeps z positive so depth stays enabled
	scale   = float32(4)
	chroma  = uint8(14)
)

var (
	pos = vec3{x: 120, y: 68}
	ang vec3

	u0          = (u1 + u3) / 2
	v0          = (v1 + v3) / 2
	transparent = []uint8{chroma}
)

type vec3 struct {
	x, y, z float32
}

// rotate applies the X, Y and Z rotation matrices to a sprite vertex and
// offsets z so the perspective divide never sees negative values.
func rotate(v vec3) vec3 {
	sinX, cosX := math.Sincos(float64(ang.x))
	sinY, cosY := math.Sincos(float64(ang.y))
	sinZ, cosZ := math.Sincos(float64(ang.z))
	sx, cx := float32(sinX), float32(cosX)
	sy, cy := float32(sinY), float32(cosY)
	sz, cz := float32(sinZ), float32(cosZ)

	y := v.y*cx - v.z*sx
	z := v.y*sx + v.z*cx

	x := v.x*cy + z*sy
	z = -v.x*sy + z*cy

	nx := x*cz - y*sz
	ny := x*sz + y*cz

	return vec3{nx, ny, z + zOffset}
}

func spriteRotation() {
	r1 := rotate(vec3{(u1 - u0) * scale, (v1 - v0) * scale, 0})
	r2 := rotate(vec3{(u2 - u0) * scale, (v2 - v0) * scale, 0})
	r3 := rotate(vec3{(u3 - u0) * scale, (v3 - v0) * scale, 0})
	r4 := rotate(vec3{(u4 - u0) * scale, (v4 - v0) * scale, 0})

	// two textured triangles forming the sprite quad; the z values enable
	// perspective correction, so the texture foreshortens when rotated
	tic80.TTri(
		r1.x+pos.x, r1.y+pos.y,
		r2.x+pos.x, r2.y+pos.y,
		r4.x+pos.x, r4.y+pos.y,
		u1, v1, u2, v2, u4, v4,
		tic80.TTriOptions{Z1: r1.z, Z2: r2.z, Z3: r4.z, Depth: true, Transparent: transparent},
	)
	tic80.TTri(
		r2.x+pos.x, r2.y+pos.y,
		r4.x+pos.x, r4.y+pos.y,
		r3.x+pos.x, r3.y+pos.y,
		u2, v2, u4, v4, u3, v3,
		tic80.TTriOptions{Z1: r2.z, Z2: r4.z, Z3: r3.z, Depth: true, Transparent: transparent},
	)
}

//go:export BOOT
func BOOT() {
	tic80.Init()
}

//go:export TIC
func TIC() {
	tic80.Cls(1)
	spriteRotation()

	if tic80.Btn(tic80.BTN_UP) {
		ang.x += 0.1
	}
	if tic80.Btn(tic80.BTN_DOWN) {
		ang.y += 0.1
	}
	if tic80.Btn(tic80.BTN_LEFT) {
		ang.z += 0.1
	}
	if tic80.Btn(tic80.BTN_RIGHT) {
		u0 += 0.2
	}

	tic80.Print("ARROWS: ROTATE / WARP", 4, 4, tic80.PrintOptions{Color: 15, Fixed: true})
}
