// API conformance probes for wren — the same sections as lua.lua, in the same
// order; that file is the reference and carries the protocol notes.
//
// Wren carts are a class, so the probes live in Game's constructor and the
// prelude is a method of it: the marker is traced, the body runs in a fiber
// whose try() catches what the binding refuses, and fiber.error names it.
// The TIC() method carries one marker of its own — the deprecated-OVR shim in
// the binding runs after every tick (vbank(1), cls(0), vbank(back)), and that
// section is where the shim shows instead of inside reset/call.

class Game is TIC {
  construct new() {
    // ---- cls
    probe("cls/default") {
      TIC.pix(10, 10, 4)
      TIC.cls()
      TIC.pix(10, 10)
    }

    probe("cls/color") {
      TIC.cls(7)
      TIC.pix(4, 4)
      TIC.pix(235, 131)
    }

    probe("cls/wrong-type") {
      TIC.cls(6)
      TIC.cls("x")
      TIC.pix(0, 0)
    }

    probe("cls/out-of-range") {
      TIC.pix(0, 0, 6)
      TIC.cls(99)
      TIC.pix(0, 0)
    }

    // ---- print
    probe("print/width") {
      TIC.print("A", 5, 5, 15)
    }

    probe("print/defaults") {
      TIC.print("A")
    }

    probe("print/scale") {
      TIC.print("A", 5, 5, 15, false, 2)
    }

    probe("print/fixed") {
      TIC.print("ii", 0, 0, 15, true)
      TIC.print("ww", 0, 10, 15, true)
    }

    probe("print/no-args") {
      TIC.print()
      TIC.trace("done")
    }

    // ---- pix
    probe("pix/write") {
      TIC.cls(0)
      TIC.pix(10, 10, 4)
      TIC.pix(10, 10)
    }

    probe("pix/read") {
      TIC.cls(2)
      TIC.pix(3, 4)
    }

    probe("pix/missing-y") {
      TIC.cls(5)
      TIC.pix(1)
      TIC.pix(1, 0)
      TIC.pix(0, 0)
    }

    probe("pix/no-args") {
      TIC.cls(3)
      TIC.pix()
      TIC.pix(0, 0)
    }

    probe("pix/float-coords") {
      TIC.cls(0)
      TIC.pix(10.7, 11.7, 4)
      TIC.pix(10, 11)
      TIC.pix(11, 11)
      TIC.pix(10, 12)
    }

    probe("pix/string-arg") {
      TIC.cls(0)
      TIC.pix("1", 2, 4)
      TIC.pix(1, 2)
    }

    probe("pix/out-of-bounds") {
      TIC.pix(300, 300, 4)
      TIC.pix(300, 300)
    }

    // ---- line
    probe("line/diagonal") {
      TIC.cls(0)
      TIC.line(0, 0, 5, 5, 6)
      TIC.pix(0, 0)
      TIC.pix(3, 3)
      TIC.pix(5, 5)
      TIC.pix(5, 0)
    }

    probe("line/floats") {
      TIC.cls(0)
      TIC.line(0.4, 1.6, 5.4, 1.6, 6)
      TIC.pix(0, 1)
      TIC.pix(0, 2)
      TIC.pix(5, 1)
      TIC.pix(5, 2)
    }

    probe("line/missing-color") {
      TIC.cls(0)
      TIC.line(0, 0, 5, 5)
      TIC.pix(0, 0)
    }

    probe("line/one-point") {
      TIC.cls(0)
      TIC.line(3, 3, 3, 3, 6)
      TIC.pix(3, 3)
    }

    // ---- rect
    probe("rect/fill") {
      TIC.cls(0)
      TIC.rect(1, 1, 3, 3, 6)
      TIC.pix(1, 1)
      TIC.pix(3, 3)
      TIC.pix(4, 4)
      TIC.pix(0, 0)
    }

    probe("rect/zero-size") {
      TIC.cls(0)
      TIC.rect(5, 5, 0, 0, 6)
      TIC.pix(5, 5)
    }

    probe("rect/negative-size") {
      TIC.cls(0)
      TIC.rect(5, 5, -2, -2, 6)
      TIC.pix(5, 5)
      TIC.pix(4, 4)
      TIC.pix(3, 3)
    }

    probe("rect/missing-color") {
      TIC.cls(0)
      TIC.rect(1, 1, 2, 2)
      TIC.pix(1, 1)
    }

    // ---- rectb
    probe("rectb/border") {
      TIC.cls(0)
      TIC.rectb(1, 1, 4, 4, 6)
      TIC.pix(1, 1)
      TIC.pix(2, 2)
      TIC.pix(4, 4)
      TIC.pix(5, 5)
    }

    probe("rectb/zero-size") {
      TIC.cls(0)
      TIC.rectb(5, 5, 0, 0, 6)
      TIC.pix(5, 5)
    }

    probe("rectb/one-wide") {
      TIC.cls(0)
      TIC.rectb(2, 2, 1, 1, 6)
      TIC.pix(2, 2)
      TIC.pix(3, 3)
    }

    probe("rectb/missing-color") {
      TIC.cls(0)
      TIC.rectb(1, 1, 2, 2)
      TIC.pix(1, 1)
    }

    // ---- rrect
    probe("rrect/fill") {
      TIC.cls(0)
      TIC.rrect(10, 10, 20, 20, 6, 4)
      TIC.pix(20, 20)
      TIC.pix(10, 20)
      TIC.pix(14, 10)
      TIC.pix(10, 10)
    }

    probe("rrect/zero-radius") {
      TIC.cls(0)
      TIC.rrect(10, 10, 20, 20, 6, 0)
      TIC.pix(10, 10)
      TIC.pix(29, 29)
      TIC.pix(30, 30)
    }

    probe("rrect/exponent") {
      TIC.cls(0)
      TIC.rrect(10, 10, 20, 20, 6, 4, 4)
      TIC.pix(12, 10)
      TIC.pix(11, 10)
      TIC.pix(10, 11)
      TIC.pix(10, 12)
    }

    probe("rrect/missing-radius") {
      TIC.cls(0)
      TIC.rrect(10, 10, 20, 20, 6)
      TIC.pix(10, 10)
    }

    // ---- rrectb
    probe("rrectb/border") {
      TIC.cls(0)
      TIC.rrectb(10, 10, 20, 20, 6, 4)
      TIC.pix(10, 20)
      TIC.pix(20, 10)
      TIC.pix(20, 20)
      TIC.pix(10, 10)
    }

    probe("rrectb/zero-radius") {
      TIC.cls(0)
      TIC.rrectb(10, 10, 20, 20, 6, 0)
      TIC.pix(10, 10)
      TIC.pix(29, 10)
      TIC.pix(20, 20)
    }

    probe("rrectb/exponent") {
      TIC.cls(0)
      TIC.rrectb(10, 10, 20, 20, 6, 4, 4)
      TIC.pix(11, 10)
      TIC.pix(12, 11)
      TIC.pix(10, 11)
      TIC.pix(12, 10)
    }

    probe("rrectb/missing-radius") {
      TIC.cls(0)
      TIC.rrectb(10, 10, 20, 20, 6)
      TIC.pix(10, 10)
    }

    // ---- mget
    probe("mget/zero") {
      TIC.mget(0, 0)
    }

    probe("mget/missing-y") {
      TIC.mget(3)
    }

    probe("mget/string-arg") {
      TIC.mget("x", 0)
    }

    // ---- mset
    probe("mset/basic") {
      TIC.mset(1, 2, 7)
      TIC.mget(1, 2)
    }

    probe("mset/out-of-range-tile") {
      TIC.mset(5, 5, 999)
      TIC.mget(5, 5)
    }

    probe("mset/missing-tile") {
      TIC.mset(6, 6, 7)
      TIC.mset(6, 6)
      TIC.mget(6, 6)
    }

    // ---- peek
    probe("peek/zero") {
      TIC.poke(8, 0)
      TIC.peek(8)
    }

    probe("peek/missing-addr") {
      TIC.peek()
    }

    probe("peek/out-of-bounds") {
      TIC.peek(1000000)
    }

    probe("peek/negative-addr") {
      TIC.peek(-1)
    }

    // ---- poke
    probe("poke/byte") {
      TIC.poke(8, 42)
      TIC.peek(8)
    }

    probe("poke/nibble") {
      TIC.poke(32, 9, 4)
      TIC.peek(32, 4)
    }

    probe("poke/missing-value") {
      TIC.poke(40, 7)
      TIC.poke(40)
      TIC.peek(40)
    }

    probe("poke/out-of-bounds") {
      TIC.poke(1000000, 1)
    }

    // ---- trace
    probe("trace/basic") {
      TIC.trace("hello")
    }

    probe("trace/color") {
      TIC.trace("hello", 5)
    }

    // ---- spr
    probe("spr/basic") {
      TIC.cls(0)
      TIC.spr(1, 10, 10)
      TIC.pix(10, 10)
    }

    probe("spr/colorkey") {
      TIC.cls(0)
      TIC.spr(1, 20, 20, 4)
      TIC.pix(20, 20)
    }

    probe("spr/scale") {
      TIC.cls(0)
      TIC.spr(1, 30, 30, -1, 2)
      TIC.pix(30, 30)
    }

    probe("spr/flip-rotate") {
      TIC.cls(0)
      TIC.spr(1, 40, 40, -1, 1, 3, 2)
      TIC.pix(40, 40)
    }

    // ---- map
    probe("map/defaults") {
      TIC.cls(0)
      TIC.map()
      TIC.pix(0, 0)
    }

    probe("map/size") {
      TIC.cls(0)
      TIC.map(0, 0, 4, 4)
      TIC.pix(0, 0)
    }

    probe("map/scale") {
      TIC.cls(0)
      TIC.map(0, 0, 4, 4, 0, 0, -1, 2)
      TIC.pix(0, 0)
    }

    // ---- btn
    probe("btn/pressed") {
      TIC.btn(0)
      TIC.btn(4)
    }

    probe("btn/missing-id") {
      TIC.btn()
    }

    // ---- btnp
    probe("btnp/pressed") {
      TIC.btnp(0)
    }

    probe("btnp/hold-period") {
      TIC.btnp(0, 10, 5)
    }

    // ---- key
    probe("key/no-key") {
      TIC.key()
    }

    probe("key/code") {
      TIC.key(1)
    }

    // ---- keyp
    probe("keyp/no-key") {
      TIC.keyp()
    }

    probe("keyp/hold-period") {
      TIC.keyp(1, 10, 5)
    }

    // ---- mouse
    probe("mouse/idle") {
      TIC.mouse()
    }

    // ---- circ
    probe("circ/fill") {
      TIC.cls(0)
      TIC.circ(20, 20, 5, 6)
      TIC.pix(20, 20)
      TIC.pix(25, 20)
    }

    probe("circ/zero-radius") {
      TIC.cls(0)
      TIC.circ(30, 30, 0, 6)
      TIC.pix(30, 30)
    }

    // ---- circb
    probe("circb/border") {
      TIC.cls(0)
      TIC.circb(20, 20, 5, 6)
      TIC.pix(20, 20)
      TIC.pix(25, 20)
    }

    probe("circb/missing-color") {
      TIC.cls(0)
      TIC.circb(20, 20, 5)
      TIC.pix(25, 20)
    }

    // ---- elli
    probe("elli/fill") {
      TIC.cls(0)
      TIC.elli(40, 40, 6, 3, 6)
      TIC.pix(40, 40)
      TIC.pix(46, 40)
    }

    probe("elli/zero-radius") {
      TIC.cls(0)
      TIC.elli(40, 40, 0, 0, 6)
      TIC.pix(40, 40)
    }

    // ---- ellib
    probe("ellib/border") {
      TIC.cls(0)
      TIC.ellib(40, 40, 6, 3, 6)
      TIC.pix(40, 40)
      TIC.pix(46, 40)
    }

    probe("ellib/missing-color") {
      TIC.cls(0)
      TIC.ellib(40, 40, 6, 3)
      TIC.pix(46, 40)
    }

    // ---- paint
    probe("paint/fill") {
      TIC.cls(0)
      TIC.paint(5, 5, 6)
      TIC.pix(0, 0)
      TIC.pix(239, 135)
    }

    probe("paint/missing-color") {
      TIC.cls(0)
      TIC.paint(5, 5)
      TIC.pix(0, 0)
    }

    // ---- tri
    probe("tri/fill") {
      TIC.cls(0)
      TIC.tri(0, 0, 10, 0, 0, 10, 6)
      TIC.pix(0, 0)
      TIC.pix(5, 0)
      TIC.pix(10, 10)
    }

    probe("tri/missing-color") {
      TIC.cls(0)
      TIC.tri(0, 0, 10, 0, 0, 10)
      TIC.pix(0, 0)
    }

    // ---- trib
    probe("trib/border") {
      TIC.cls(0)
      TIC.trib(0, 0, 10, 0, 0, 10, 6)
      TIC.pix(0, 0)
      TIC.pix(5, 0)
    }

    probe("trib/missing-color") {
      TIC.cls(0)
      TIC.trib(0, 0, 10, 0, 0, 10)
      TIC.pix(0, 0)
    }

    // ---- ttri
    probe("ttri/basic") {
      TIC.cls(0)
      TIC.ttri(0, 0, 10, 0, 0, 10, 0, 0, 1, 0, 0, 1)
      TIC.pix(0, 0)
      TIC.pix(5, 0)
    }

    probe("ttri/chromakey") {
      TIC.cls(0)
      TIC.ttri(0, 0, 10, 0, 0, 10, 0, 0, 1, 0, 0, 1, 0, 4)
      TIC.pix(0, 0)
    }

    // ---- clip
    probe("clip/set") {
      TIC.cls(0)
      TIC.clip(2, 2, 5, 5)
      TIC.rect(0, 0, 10, 10, 6)
      TIC.pix(0, 0)
      TIC.pix(2, 2)
      TIC.clip()
    }

    probe("clip/reset") {
      TIC.cls(0)
      TIC.clip()
      TIC.rect(0, 0, 2, 2, 6)
      TIC.pix(0, 0)
    }

    // ---- font
    probe("font/text") {
      TIC.cls(0)
      TIC.font("ab", 0, 0, -1, 6, 6)
      TIC.pix(0, 0)
    }

    // ---- sfx
    probe("sfx/basic") {
      TIC.sfx(0)
    }

    probe("sfx/note") {
      TIC.sfx(0, 48)
    }

    probe("sfx/channel") {
      TIC.sfx(0, 48, -1, 1)
    }

    probe("sfx/missing-id") {
      TIC.sfx()
    }

    // ---- music
    probe("music/basic") {
      TIC.music(0)
    }

    probe("music/frame") {
      TIC.music(0, 0, 0, true, false)
    }

    // ---- fget
    probe("fget/out-of-range") {
      TIC.fget(500, 0)
    }

    // ---- fset
    probe("fset/roundtrip") {
      TIC.fset(0, 0, true)
      TIC.fget(0, 0)
    }

    probe("fset/missing-bool") {
      TIC.fset(0, 1)
      TIC.fget(0, 1)
    }

    // ---- peek1
    probe("peek1/read") {
      TIC.peek1(16)
    }

    // ---- poke1
    probe("poke1/roundtrip") {
      TIC.poke1(16, 7)
      TIC.peek1(16)
    }

    // ---- peek2
    probe("peek2/read") {
      TIC.peek2(24)
    }

    // ---- poke2
    probe("poke2/roundtrip") {
      TIC.poke2(24, 300)
      TIC.peek2(24)
    }

    // ---- peek4
    probe("peek4/read") {
      TIC.peek4(64)
    }

    // ---- poke4
    probe("poke4/roundtrip") {
      TIC.poke4(64, 9)
      TIC.peek4(64)
    }

    // ---- memset
    probe("memset/bytes") {
      TIC.memset(48, 7, 2)
      TIC.peek(48)
      TIC.peek(49)
    }

    // ---- memcpy
    probe("memcpy/copy") {
      TIC.poke(56, 1)
      TIC.poke(57, 2)
      TIC.memcpy(60, 56, 2)
      TIC.peek(60)
      TIC.peek(61)
    }

    // ---- pmem
    probe("pmem/read") {
      TIC.pmem(0)
    }

    probe("pmem/write-read") {
      TIC.pmem(0, 42)
      TIC.pmem(0)
    }

    probe("pmem/out-of-range") {
      TIC.pmem(1000)
    }

    // ---- time
    probe("time/stub") {
      TIC.time()
    }

    // ---- tstamp
    probe("tstamp/stub") {
      TIC.tstamp()
    }

    // ---- fft
    probe("fft/sample") {
      TIC.fft(0)
    }

    // ---- ffts
    probe("ffts/spectrum") {
      TIC.ffts(0)
    }

    // ---- fftr
    probe("fftr/range") {
      TIC.fftr(0, 100)
    }

    // ---- fftrs
    probe("fftrs/range") {
      TIC.fftrs(0, 100)
    }

    // ---- vqt
    probe("vqt/read") {
      TIC.vqt(0)
    }

    // ---- vqts
    probe("vqts/read") {
      TIC.vqts(0)
    }

    // ---- vqtr
    probe("vqtr/read") {
      TIC.vqtr(0)
    }

    // ---- vqtrs
    probe("vqtrs/read") {
      TIC.vqtrs(0)
    }

    // ---- vqtw
    probe("vqtw/read") {
      TIC.vqtw(0)
    }

    // ---- vqtsw
    probe("vqtsw/read") {
      TIC.vqtsw(0)
    }

    // ---- vqtrw
    probe("vqtrw/read") {
      TIC.vqtrw(0)
    }

    // ---- vqtrsw
    probe("vqtrsw/read") {
      TIC.vqtrsw(0)
    }

    // ---- vbank
    probe("vbank/switch") {
      TIC.vbank(1)
      TIC.vbank(0)
    }

    probe("vbank/no-args") {
      TIC.vbank()
      TIC.trace("done")
    }

    // ---- sync
    probe("sync/defaults") {
      TIC.sync()
    }

    probe("sync/mask") {
      TIC.sync(32767, 0, false)
    }

    // ---- exit
    probe("exit/call") {
      TIC.exit()
    }

    // ---- reset
    probe("reset/call") {
      TIC.reset()
      TIC.trace("done")
    }
  }

  probe(id, f) {
    TIC.trace("#" + id)
    var fiber = Fiber.new(f)
    fiber.try()
    if (fiber.error != null) {
      TIC.trace("!error")
    }
  }

  TIC() {
    TIC.trace("#wren-bind/overlay-shim")
  }
}
