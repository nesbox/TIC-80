# API conformance probes for janet — the same sections as lua.lua, in the same
# order; that file is the reference and carries the protocol notes.
#
# The api is a module, so every call is tic80/name; the prelude traces the
# marker and runs each body under try/catch. Janet resolves symbols at compile
# time, so a body can only name functions that exist — a language without one
# would write !missing instead of the call, as a section it cannot express.

(import tic80)

(defn probe [id f]
  (tic80/trace (string "#" id))

  (try (f) ([e] (tic80/trace "!error"))))

# ---- cls

(probe "cls/default" (fn []
  (tic80/pix 10 10 4)
  (tic80/cls)
  (tic80/pix 10 10)
  ))

(probe "cls/color" (fn []
  (tic80/cls 7)
  (tic80/pix 4 4)
  (tic80/pix 235 131)
  ))

(probe "cls/wrong-type" (fn []
  (tic80/cls 6)
  (tic80/cls "x")
  (tic80/pix 0 0)
  ))

(probe "cls/out-of-range" (fn []
  (tic80/pix 0 0 6)
  (tic80/cls 99)
  (tic80/pix 0 0)
  ))

# ---- print

(probe "print/width" (fn []
  (tic80/print "A" 5 5 15)
  ))

(probe "print/defaults" (fn []
  (tic80/print "A")
  ))

(probe "print/scale" (fn []
  (tic80/print "A" 5 5 15 false 2)
  ))

(probe "print/fixed" (fn []
  (tic80/print "ii" 0 0 15 true)
  (tic80/print "ww" 0 10 15 true)
  ))

(probe "print/no-args" (fn []
  (tic80/print)
  (tic80/trace "done")
  ))

# ---- pix

(probe "pix/write" (fn []
  (tic80/cls 0)
  (tic80/pix 10 10 4)
  (tic80/pix 10 10)
  ))

(probe "pix/read" (fn []
  (tic80/cls 2)
  (tic80/pix 3 4)
  ))

(probe "pix/missing-y" (fn []
  (tic80/cls 5)
  (tic80/pix 1)
  (tic80/pix 1 0)
  (tic80/pix 0 0)
  ))

(probe "pix/no-args" (fn []
  (tic80/cls 3)
  (tic80/pix)
  (tic80/pix 0 0)
  ))

(probe "pix/float-coords" (fn []
  (tic80/cls 0)
  (tic80/pix 10.7 11.7 4)
  (tic80/pix 10 11)
  (tic80/pix 11 11)
  (tic80/pix 10 12)
  ))

(probe "pix/string-arg" (fn []
  (tic80/cls 0)
  (tic80/pix "1" 2 4)
  (tic80/pix 1 2)
  ))

(probe "pix/out-of-bounds" (fn []
  (tic80/pix 300 300 4)
  (tic80/pix 300 300)
  ))

# ---- line

(probe "line/diagonal" (fn []
  (tic80/cls 0)
  (tic80/line 0 0 5 5 6)
  (tic80/pix 0 0)
  (tic80/pix 3 3)
  (tic80/pix 5 5)
  (tic80/pix 5 0)
  ))

(probe "line/floats" (fn []
  (tic80/cls 0)
  (tic80/line 0.4 1.6 5.4 1.6 6)
  (tic80/pix 0 1)
  (tic80/pix 0 2)
  (tic80/pix 5 1)
  (tic80/pix 5 2)
  ))

(probe "line/missing-color" (fn []
  (tic80/cls 0)
  (tic80/line 0 0 5 5)
  (tic80/pix 0 0)
  ))

(probe "line/one-point" (fn []
  (tic80/cls 0)
  (tic80/line 3 3 3 3 6)
  (tic80/pix 3 3)
  ))

# ---- rect

(probe "rect/fill" (fn []
  (tic80/cls 0)
  (tic80/rect 1 1 3 3 6)
  (tic80/pix 1 1)
  (tic80/pix 3 3)
  (tic80/pix 4 4)
  (tic80/pix 0 0)
  ))

(probe "rect/zero-size" (fn []
  (tic80/cls 0)
  (tic80/rect 5 5 0 0 6)
  (tic80/pix 5 5)
  ))

(probe "rect/negative-size" (fn []
  (tic80/cls 0)
  (tic80/rect 5 5 -2 -2 6)
  (tic80/pix 5 5)
  (tic80/pix 4 4)
  (tic80/pix 3 3)
  ))

(probe "rect/missing-color" (fn []
  (tic80/cls 0)
  (tic80/rect 1 1 2 2)
  (tic80/pix 1 1)
  ))

# ---- rectb

(probe "rectb/border" (fn []
  (tic80/cls 0)
  (tic80/rectb 1 1 4 4 6)
  (tic80/pix 1 1)
  (tic80/pix 2 2)
  (tic80/pix 4 4)
  (tic80/pix 5 5)
  ))

(probe "rectb/zero-size" (fn []
  (tic80/cls 0)
  (tic80/rectb 5 5 0 0 6)
  (tic80/pix 5 5)
  ))

(probe "rectb/one-wide" (fn []
  (tic80/cls 0)
  (tic80/rectb 2 2 1 1 6)
  (tic80/pix 2 2)
  (tic80/pix 3 3)
  ))

(probe "rectb/missing-color" (fn []
  (tic80/cls 0)
  (tic80/rectb 1 1 2 2)
  (tic80/pix 1 1)
  ))

# ---- rrect

(probe "rrect/fill" (fn []
  (tic80/cls 0)
  (tic80/rrect 10 10 20 20 6 4)
  (tic80/pix 20 20)
  (tic80/pix 10 20)
  (tic80/pix 14 10)
  (tic80/pix 10 10)
  ))

(probe "rrect/zero-radius" (fn []
  (tic80/cls 0)
  (tic80/rrect 10 10 20 20 6 0)
  (tic80/pix 10 10)
  (tic80/pix 29 29)
  (tic80/pix 30 30)
  ))

(probe "rrect/exponent" (fn []
  (tic80/cls 0)
  (tic80/rrect 10 10 20 20 6 4 4)
  (tic80/pix 12 10)
  (tic80/pix 11 10)
  (tic80/pix 10 11)
  (tic80/pix 10 12)
  ))

(probe "rrect/missing-radius" (fn []
  (tic80/cls 0)
  (tic80/rrect 10 10 20 20 6)
  (tic80/pix 10 10)
  ))

# ---- rrectb

(probe "rrectb/border" (fn []
  (tic80/cls 0)
  (tic80/rrectb 10 10 20 20 6 4)
  (tic80/pix 10 20)
  (tic80/pix 20 10)
  (tic80/pix 20 20)
  (tic80/pix 10 10)
  ))

(probe "rrectb/zero-radius" (fn []
  (tic80/cls 0)
  (tic80/rrectb 10 10 20 20 6 0)
  (tic80/pix 10 10)
  (tic80/pix 29 10)
  (tic80/pix 20 20)
  ))

(probe "rrectb/exponent" (fn []
  (tic80/cls 0)
  (tic80/rrectb 10 10 20 20 6 4 4)
  (tic80/pix 11 10)
  (tic80/pix 12 11)
  (tic80/pix 10 11)
  (tic80/pix 12 10)
  ))

(probe "rrectb/missing-radius" (fn []
  (tic80/cls 0)
  (tic80/rrectb 10 10 20 20 6)
  (tic80/pix 10 10)
  ))

# ---- mget

(probe "mget/zero" (fn []
  (tic80/mget 0 0)
  ))

(probe "mget/missing-y" (fn []
  (tic80/mget 3)
  ))

(probe "mget/string-arg" (fn []
  (tic80/mget "x" 0)
  ))

# ---- mset

(probe "mset/basic" (fn []
  (tic80/mset 1 2 7)
  (tic80/mget 1 2)
  ))

(probe "mset/out-of-range-tile" (fn []
  (tic80/mset 5 5 999)
  (tic80/mget 5 5)
  ))

(probe "mset/missing-tile" (fn []
  (tic80/mset 6 6 7)
  (tic80/mset 6 6)
  (tic80/mget 6 6)
  ))

# ---- peek

(probe "peek/zero" (fn []
  (tic80/poke 8 0)
  (tic80/peek 8)
  ))

(probe "peek/missing-addr" (fn []
  (tic80/peek)
  ))

(probe "peek/out-of-bounds" (fn []
  (tic80/peek 1000000)
  ))

(probe "peek/negative-addr" (fn []
  (tic80/peek -1)
  ))

# ---- poke

(probe "poke/byte" (fn []
  (tic80/poke 8 42)
  (tic80/peek 8)
  ))

(probe "poke/nibble" (fn []
  (tic80/poke 32 9 4)
  (tic80/peek 32 4)
  ))

(probe "poke/missing-value" (fn []
  (tic80/poke 40 7)
  (tic80/poke 40)
  (tic80/peek 40)
  ))

(probe "poke/out-of-bounds" (fn []
  (tic80/poke 1000000 1)
  ))

# ---- trace

(probe "trace/basic" (fn []
  (tic80/trace "hello")
  ))

(probe "trace/color" (fn []
  (tic80/trace "hello" 5)
  ))

# ---- spr

(probe "spr/basic" (fn []
  (tic80/cls 0)
  (tic80/spr 1 10 10)
  (tic80/pix 10 10)
  ))

(probe "spr/colorkey" (fn []
  (tic80/cls 0)
  (tic80/spr 1 20 20 4)
  (tic80/pix 20 20)
  ))

(probe "spr/scale" (fn []
  (tic80/cls 0)
  (tic80/spr 1 30 30 -1 2)
  (tic80/pix 30 30)
  ))

(probe "spr/flip-rotate" (fn []
  (tic80/cls 0)
  (tic80/spr 1 40 40 -1 1 3 2)
  (tic80/pix 40 40)
  ))

# ---- map

(probe "map/defaults" (fn []
  (tic80/cls 0)
  (tic80/map)
  (tic80/pix 0 0)
  ))

(probe "map/size" (fn []
  (tic80/cls 0)
  (tic80/map 0 0 4 4)
  (tic80/pix 0 0)
  ))

(probe "map/scale" (fn []
  (tic80/cls 0)
  (tic80/map 0 0 4 4 0 0 -1 2)
  (tic80/pix 0 0)
  ))

# ---- btn

(probe "btn/pressed" (fn []
  (tic80/btn 0)
  (tic80/btn 4)
  ))

(probe "btn/missing-id" (fn []
  (tic80/btn)
  ))

# ---- btnp

(probe "btnp/pressed" (fn []
  (tic80/btnp 0)
  ))

(probe "btnp/hold-period" (fn []
  (tic80/btnp 0 10 5)
  ))

# ---- btnr

(probe "btnr/released" (fn []
  (tic80/btnr 0)
  ))

(probe "btnr/missing-id" (fn []
  (tic80/btnr)
  ))

# ---- key

(probe "key/no-key" (fn []
  (tic80/key)
  ))

(probe "key/code" (fn []
  (tic80/key 1)
  ))

# ---- keyp

(probe "keyp/no-key" (fn []
  (tic80/keyp)
  ))

(probe "keyp/hold-period" (fn []
  (tic80/keyp 1 10 5)
  ))

# ---- keyr

(probe "keyr/no-key" (fn []
  (tic80/keyr)
  ))

(probe "keyr/code" (fn []
  (tic80/keyr 1)
  ))

# ---- mouse

(probe "mouse/idle" (fn []
  (tic80/mouse)
  ))

# ---- circ

(probe "circ/fill" (fn []
  (tic80/cls 0)
  (tic80/circ 20 20 5 6)
  (tic80/pix 20 20)
  (tic80/pix 25 20)
  ))

(probe "circ/zero-radius" (fn []
  (tic80/cls 0)
  (tic80/circ 30 30 0 6)
  (tic80/pix 30 30)
  ))

# ---- circb

(probe "circb/border" (fn []
  (tic80/cls 0)
  (tic80/circb 20 20 5 6)
  (tic80/pix 20 20)
  (tic80/pix 25 20)
  ))

(probe "circb/missing-color" (fn []
  (tic80/cls 0)
  (tic80/circb 20 20 5)
  (tic80/pix 25 20)
  ))

# ---- elli

(probe "elli/fill" (fn []
  (tic80/cls 0)
  (tic80/elli 40 40 6 3 6)
  (tic80/pix 40 40)
  (tic80/pix 46 40)
  ))

(probe "elli/zero-radius" (fn []
  (tic80/cls 0)
  (tic80/elli 40 40 0 0 6)
  (tic80/pix 40 40)
  ))

# ---- ellib

(probe "ellib/border" (fn []
  (tic80/cls 0)
  (tic80/ellib 40 40 6 3 6)
  (tic80/pix 40 40)
  (tic80/pix 46 40)
  ))

(probe "ellib/missing-color" (fn []
  (tic80/cls 0)
  (tic80/ellib 40 40 6 3)
  (tic80/pix 46 40)
  ))

# ---- paint

(probe "paint/fill" (fn []
  (tic80/cls 0)
  (tic80/paint 5 5 6)
  (tic80/pix 0 0)
  (tic80/pix 239 135)
  ))

(probe "paint/missing-color" (fn []
  (tic80/cls 0)
  (tic80/paint 5 5)
  (tic80/pix 0 0)
  ))

# ---- tri

(probe "tri/fill" (fn []
  (tic80/cls 0)
  (tic80/tri 0 0 10 0 0 10 6)
  (tic80/pix 0 0)
  (tic80/pix 5 0)
  (tic80/pix 10 10)
  ))

(probe "tri/missing-color" (fn []
  (tic80/cls 0)
  (tic80/tri 0 0 10 0 0 10)
  (tic80/pix 0 0)
  ))

# ---- trib

(probe "trib/border" (fn []
  (tic80/cls 0)
  (tic80/trib 0 0 10 0 0 10 6)
  (tic80/pix 0 0)
  (tic80/pix 5 0)
  ))

(probe "trib/missing-color" (fn []
  (tic80/cls 0)
  (tic80/trib 0 0 10 0 0 10)
  (tic80/pix 0 0)
  ))

# ---- ttri

(probe "ttri/basic" (fn []
  (tic80/cls 0)
  (tic80/ttri 0 0 10 0 0 10 0 0 1 0 0 1)
  (tic80/pix 0 0)
  (tic80/pix 5 0)
  ))

(probe "ttri/chromakey" (fn []
  (tic80/cls 0)
  (tic80/ttri 0 0 10 0 0 10 0 0 1 0 0 1 0 4)
  (tic80/pix 0 0)
  ))

# ---- clip

(probe "clip/set" (fn []
  (tic80/cls 0)
  (tic80/clip 2 2 5 5)
  (tic80/rect 0 0 10 10 6)
  (tic80/pix 0 0)
  (tic80/pix 2 2)
  (tic80/clip)
  ))

(probe "clip/reset" (fn []
  (tic80/cls 0)
  (tic80/clip)
  (tic80/rect 0 0 2 2 6)
  (tic80/pix 0 0)
  ))

# ---- font

(probe "font/text" (fn []
  (tic80/cls 0)
  (tic80/font "ab" 0 0 -1 6 6)
  (tic80/pix 0 0)
  ))

# ---- sfx

(probe "sfx/basic" (fn []
  (tic80/sfx 0)
  ))

(probe "sfx/note" (fn []
  (tic80/sfx 0 48)
  ))

(probe "sfx/channel" (fn []
  (tic80/sfx 0 48 -1 1)
  ))

(probe "sfx/missing-id" (fn []
  (tic80/sfx)
  ))

# ---- music

(probe "music/basic" (fn []
  (tic80/music 0)
  ))

(probe "music/frame" (fn []
  (tic80/music 0 0 0 true false)
  ))

# ---- fget

(probe "fget/out-of-range" (fn []
  (tic80/fget 500 0)
  ))

# ---- fset

(probe "fset/roundtrip" (fn []
  (tic80/fset 0 0 true)
  (tic80/fget 0 0)
  ))

(probe "fset/missing-bool" (fn []
  (tic80/fset 0 1)
  (tic80/fget 0 1)
  ))

# ---- peek1

(probe "peek1/read" (fn []
  (tic80/peek1 16)
  ))

# ---- poke1

(probe "poke1/roundtrip" (fn []
  (tic80/poke1 16 7)
  (tic80/peek1 16)
  ))

# ---- peek2

(probe "peek2/read" (fn []
  (tic80/peek2 24)
  ))

# ---- poke2

(probe "poke2/roundtrip" (fn []
  (tic80/poke2 24 300)
  (tic80/peek2 24)
  ))

# ---- peek4

(probe "peek4/read" (fn []
  (tic80/peek4 64)
  ))

# ---- poke4

(probe "poke4/roundtrip" (fn []
  (tic80/poke4 64 9)
  (tic80/peek4 64)
  ))

# ---- memset

(probe "memset/bytes" (fn []
  (tic80/memset 48 7 2)
  (tic80/peek 48)
  (tic80/peek 49)
  ))

# ---- memcpy

(probe "memcpy/copy" (fn []
  (tic80/poke 56 1)
  (tic80/poke 57 2)
  (tic80/memcpy 60 56 2)
  (tic80/peek 60)
  (tic80/peek 61)
  ))

# ---- pmem

(probe "pmem/read" (fn []
  (tic80/pmem 0)
  ))

(probe "pmem/write-read" (fn []
  (tic80/pmem 0 42)
  (tic80/pmem 0)
  ))

(probe "pmem/out-of-range" (fn []
  (tic80/pmem 1000)
  ))

# ---- time

(probe "time/stub" (fn []
  (tic80/time)
  ))

# ---- tstamp

(probe "tstamp/stub" (fn []
  (tic80/tstamp)
  ))

# ---- fft

(probe "fft/sample" (fn []
  (tic80/fft 0)
  ))

# ---- ffts

(probe "ffts/spectrum" (fn []
  (tic80/ffts 0)
  ))

# ---- fftr

(probe "fftr/range" (fn []
  (tic80/fftr 0 100)
  ))

# ---- fftrs

(probe "fftrs/range" (fn []
  (tic80/fftrs 0 100)
  ))

# ---- vqt

(probe "vqt/read" (fn []
  (tic80/vqt 0)
  ))

# ---- vqts

(probe "vqts/read" (fn []
  (tic80/vqts 0)
  ))

# ---- vqtr

(probe "vqtr/read" (fn []
  (tic80/vqtr 0)
  ))

# ---- vqtrs

(probe "vqtrs/read" (fn []
  (tic80/vqtrs 0)
  ))

# ---- vqtw

(probe "vqtw/read" (fn []
  (tic80/vqtw 0)
  ))

# ---- vqtsw

(probe "vqtsw/read" (fn []
  (tic80/vqtsw 0)
  ))

# ---- vqtrw

(probe "vqtrw/read" (fn []
  (tic80/vqtrw 0)
  ))

# ---- vqtrsw

(probe "vqtrsw/read" (fn []
  (tic80/vqtrsw 0)
  ))

# ---- vbank

(probe "vbank/switch" (fn []
  (tic80/vbank 1)
  (tic80/vbank 0)
  ))

(probe "vbank/no-args" (fn []
  (tic80/vbank)
  (tic80/trace "done")
  ))

# ---- sync

(probe "sync/defaults" (fn []
  (tic80/sync)
  ))

(probe "sync/mask" (fn []
  (tic80/sync 32767 0 false)
  ))

# ---- exit

(probe "exit/call" (fn []
  (tic80/exit)
  ))

# ---- reset

(probe "reset/call" (fn []
  (tic80/reset)
  (tic80/trace "done")
  ))

# The engine runs a cart's TIC every tick; the probes live at the top level.
(defn TIC [] nil)
