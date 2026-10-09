;; API conformance probes for fennel — the same sections as lua.lua, in the
;; same order; that file is the reference and carries the protocol notes.
;;
;; probe(id, f) traces the marker, then runs f. The runner records every API
;; call with the values it arrived with. TIC goes to the global table because
;; fennel declares locals by default and the engine looks the callback up as
;; a global.

(fn probe [id f]
  (trace (.. "#" id))
  (let [name (: id :match "^([^/]+)")]
    (if (= :function (type (. _G name)))
        (when (not (pcall f)) (trace "!error"))
        (trace "!missing"))))

;; cls

(probe :cls/default
  (fn []
    (pix 10 10 4)
    (cls)
    (pix 10 10)
    ))

(probe :cls/color
  (fn []
    (cls 7)
    (pix 4 4)
    (pix 235 131)
    ))

(probe :cls/wrong-type
  (fn []
    (cls 6)
    (cls "x")
    (pix 0 0)
    ))

(probe :cls/out-of-range
  (fn []
    (pix 0 0 6)
    (cls 99)
    (pix 0 0)
    ))

;; print

(probe :print/width
  (fn []
    (print "A" 5 5 15)
    ))

(probe :print/defaults
  (fn []
    (print "A")
    ))

(probe :print/scale
  (fn []
    (print "A" 5 5 15 false 2)
    ))

(probe :print/fixed
  (fn []
    (print "ii" 0 0 15 true)
    (print "ww" 0 10 15 true)
    ))

(probe :print/no-args
  (fn []
    (print)
    ))

;; pix

(probe :pix/write
  (fn []
    (cls 0)
    (pix 10 10 4)
    (pix 10 10)
    ))

(probe :pix/read
  (fn []
    (cls 2)
    (pix 3 4)
    ))

(probe :pix/missing-y
  (fn []
    (cls 5)
    (pix 1)
    (pix 1 0)
    (pix 0 0)
    ))

(probe :pix/no-args
  (fn []
    (cls 3)
    (pix)
    (pix 0 0)
    ))

(probe :pix/float-coords
  (fn []
    (cls 0)
    (pix 10.7 11.7 4)
    (pix 10 11)
    (pix 11 11)
    (pix 10 12)
    ))

(probe :pix/string-arg
  (fn []
    (cls 0)
    (pix "1" 2 4)
    (pix 1 2)
    ))

(probe :pix/out-of-bounds
  (fn []
    (pix 300 300 4)
    (pix 300 300)
    ))

;; line

(probe :line/diagonal
  (fn []
    (cls 0)
    (line 0 0 5 5 6)
    (pix 0 0)
    (pix 3 3)
    (pix 5 5)
    (pix 5 0)
    ))

(probe :line/floats
  (fn []
    (cls 0)
    (line 0.4 1.6 5.4 1.6 6)
    (pix 0 1)
    (pix 0 2)
    (pix 5 1)
    (pix 5 2)
    ))

(probe :line/missing-color
  (fn []
    (cls 0)
    (line 0 0 5 5)
    (pix 0 0)
    ))

(probe :line/one-point
  (fn []
    (cls 0)
    (line 3 3 3 3 6)
    (pix 3 3)
    ))

;; rect

(probe :rect/fill
  (fn []
    (cls 0)
    (rect 1 1 3 3 6)
    (pix 1 1)
    (pix 3 3)
    (pix 4 4)
    (pix 0 0)
    ))

(probe :rect/zero-size
  (fn []
    (cls 0)
    (rect 5 5 0 0 6)
    (pix 5 5)
    ))

(probe :rect/negative-size
  (fn []
    (cls 0)
    (rect 5 5 -2 -2 6)
    (pix 5 5)
    (pix 4 4)
    (pix 3 3)
    ))

(probe :rect/missing-color
  (fn []
    (cls 0)
    (rect 1 1 2 2)
    (pix 1 1)
    ))

;; rectb

(probe :rectb/border
  (fn []
    (cls 0)
    (rectb 1 1 4 4 6)
    (pix 1 1)
    (pix 2 2)
    (pix 4 4)
    (pix 5 5)
    ))

(probe :rectb/zero-size
  (fn []
    (cls 0)
    (rectb 5 5 0 0 6)
    (pix 5 5)
    ))

(probe :rectb/one-wide
  (fn []
    (cls 0)
    (rectb 2 2 1 1 6)
    (pix 2 2)
    (pix 3 3)
    ))

(probe :rectb/missing-color
  (fn []
    (cls 0)
    (rectb 1 1 2 2)
    (pix 1 1)
    ))

;; mget

(probe :mget/zero
  (fn []
    (mget 0 0)
    ))

(probe :mget/missing-y
  (fn []
    (mget 3)
    ))

(probe :mget/string-arg
  (fn []
    (mget "x" 0)
    ))

;; mset

(probe :mset/basic
  (fn []
    (mset 1 2 7)
    (mget 1 2)
    ))

(probe :mset/out-of-range-tile
  (fn []
    (mset 5 5 999)
    (mget 5 5)
    ))

(probe :mset/missing-tile
  (fn []
    (mset 6 6 7)
    (mset 6 6)
    (mget 6 6)
    ))

;; peek

(probe :peek/zero
  (fn []
    (poke 8 0)
    (peek 8)
    ))

(probe :peek/missing-addr
  (fn []
    (peek)
    ))

(probe :peek/out-of-bounds
  (fn []
    (peek 1000000)
    ))

(probe :peek/negative-addr
  (fn []
    (peek -1)
    ))

;; poke

(probe :poke/byte
  (fn []
    (poke 8 42)
    (peek 8)
    ))

(probe :poke/nibble
  (fn []
    (poke 32 9 4)
    (peek 32 4)
    ))

(probe :poke/missing-value
  (fn []
    (poke 40 7)
    (poke 40)
    (peek 40)
    ))

(probe :poke/out-of-bounds
  (fn []
    (poke 1000000 1)
    ))

;; trace

(probe :trace/basic
  (fn []
    (trace "hello")
    ))

(probe :trace/color
  (fn []
    (trace "hello" 5)
    ))

;; spr

(probe :spr/basic
  (fn []
    (cls 0)
    (spr 1 10 10)
    (pix 10 10)
    ))

(probe :spr/colorkey
  (fn []
    (cls 0)
    (spr 1 20 20 4)
    (pix 20 20)
    ))

(probe :spr/scale
  (fn []
    (cls 0)
    (spr 1 30 30 -1 2)
    (pix 30 30)
    ))

(probe :spr/flip-rotate
  (fn []
    (cls 0)
    (spr 1 40 40 -1 1 3 2)
    (pix 40 40)
    ))

;; map

(probe :map/defaults
  (fn []
    (cls 0)
    (map)
    (pix 0 0)
    ))

(probe :map/size
  (fn []
    (cls 0)
    (map 0 0 4 4)
    (pix 0 0)
    ))

(probe :map/scale
  (fn []
    (cls 0)
    (map 0 0 4 4 0 0 -1 2)
    (pix 0 0)
    ))

;; btn

(probe :btn/pressed
  (fn []
    (btn 0)
    (btn 4)
    ))

(probe :btn/missing-id
  (fn []
    (btn)
    ))

;; btnp

(probe :btnp/pressed
  (fn []
    (btnp 0)
    ))

(probe :btnp/hold-period
  (fn []
    (btnp 0 10 5)
    ))

;; key

(probe :key/no-key
  (fn []
    (key)
    ))

(probe :key/code
  (fn []
    (key 1)
    ))

;; keyp

(probe :keyp/no-key
  (fn []
    (keyp)
    ))

(probe :keyp/hold-period
  (fn []
    (keyp 1 10 5)
    ))

;; mouse

(probe :mouse/idle
  (fn []
    (mouse)
    ))

;; circ

(probe :circ/fill
  (fn []
    (cls 0)
    (circ 20 20 5 6)
    (pix 20 20)
    (pix 25 20)
    ))

(probe :circ/zero-radius
  (fn []
    (cls 0)
    (circ 30 30 0 6)
    (pix 30 30)
    ))

;; circb

(probe :circb/border
  (fn []
    (cls 0)
    (circb 20 20 5 6)
    (pix 20 20)
    (pix 25 20)
    ))

(probe :circb/missing-color
  (fn []
    (cls 0)
    (circb 20 20 5)
    (pix 25 20)
    ))

;; elli

(probe :elli/fill
  (fn []
    (cls 0)
    (elli 40 40 6 3 6)
    (pix 40 40)
    (pix 46 40)
    ))

(probe :elli/zero-radius
  (fn []
    (cls 0)
    (elli 40 40 0 0 6)
    (pix 40 40)
    ))

;; ellib

(probe :ellib/border
  (fn []
    (cls 0)
    (ellib 40 40 6 3 6)
    (pix 40 40)
    (pix 46 40)
    ))

(probe :ellib/missing-color
  (fn []
    (cls 0)
    (ellib 40 40 6 3)
    (pix 46 40)
    ))

;; paint

(probe :paint/fill
  (fn []
    (cls 0)
    (paint 5 5 6)
    (pix 0 0)
    (pix 239 135)
    ))

(probe :paint/missing-color
  (fn []
    (cls 0)
    (paint 5 5)
    (pix 0 0)
    ))

;; tri

(probe :tri/fill
  (fn []
    (cls 0)
    (tri 0 0 10 0 0 10 6)
    (pix 0 0)
    (pix 5 0)
    (pix 10 10)
    ))

(probe :tri/missing-color
  (fn []
    (cls 0)
    (tri 0 0 10 0 0 10)
    (pix 0 0)
    ))

;; trib

(probe :trib/border
  (fn []
    (cls 0)
    (trib 0 0 10 0 0 10 6)
    (pix 0 0)
    (pix 5 0)
    ))

(probe :trib/missing-color
  (fn []
    (cls 0)
    (trib 0 0 10 0 0 10)
    (pix 0 0)
    ))

;; ttri

(probe :ttri/basic
  (fn []
    (cls 0)
    (ttri 0 0 10 0 0 10 0 0 1 0 0 1)
    (pix 0 0)
    (pix 5 0)
    ))

(probe :ttri/chromakey
  (fn []
    (cls 0)
    (ttri 0 0 10 0 0 10 0 0 1 0 0 1 0 4)
    (pix 0 0)
    ))

;; clip

(probe :clip/set
  (fn []
    (cls 0)
    (clip 2 2 5 5)
    (rect 0 0 10 10 6)
    (pix 0 0)
    (pix 2 2)
    (clip)
    ))

(probe :clip/reset
  (fn []
    (cls 0)
    (clip)
    (rect 0 0 2 2 6)
    (pix 0 0)
    ))

;; font

(probe :font/text
  (fn []
    (cls 0)
    (font "ab" 0 0 -1 6 6)
    (pix 0 0)
    ))

;; sfx

(probe :sfx/basic
  (fn []
    (sfx 0)
    ))

(probe :sfx/note
  (fn []
    (sfx 0 48)
    ))

(probe :sfx/channel
  (fn []
    (sfx 0 48 -1 1)
    ))

(probe :sfx/missing-id
  (fn []
    (sfx)
    ))

;; music

(probe :music/basic
  (fn []
    (music 0)
    ))

(probe :music/frame
  (fn []
    (music 0 0 0 true false)
    ))

;; fget

(probe :fget/out-of-range
  (fn []
    (fget 500 0)
    ))

;; fset

(probe :fset/roundtrip
  (fn []
    (fset 0 0 true)
    (fget 0 0)
    ))

(probe :fset/missing-bool
  (fn []
    (fset 0 1)
    (fget 0 1)
    ))

;; peek1

(probe :peek1/read
  (fn []
    (peek1 16)
    ))

;; poke1

(probe :poke1/roundtrip
  (fn []
    (poke1 16 7)
    (peek1 16)
    ))

;; peek2

(probe :peek2/read
  (fn []
    (peek2 24)
    ))

;; poke2

(probe :poke2/roundtrip
  (fn []
    (poke2 24 300)
    (peek2 24)
    ))

;; peek4

(probe :peek4/read
  (fn []
    (peek4 64)
    ))

;; poke4

(probe :poke4/roundtrip
  (fn []
    (poke4 64 9)
    (peek4 64)
    ))

;; memset

(probe :memset/bytes
  (fn []
    (memset 48 7 2)
    (peek 48)
    (peek 49)
    ))

;; memcpy

(probe :memcpy/copy
  (fn []
    (poke 56 1)
    (poke 57 2)
    (memcpy 60 56 2)
    (peek 60)
    (peek 61)
    ))

;; pmem

(probe :pmem/read
  (fn []
    (pmem 0)
    ))

(probe :pmem/write-read
  (fn []
    (pmem 0 42)
    (pmem 0)
    ))

(probe :pmem/out-of-range
  (fn []
    (pmem 1000)
    ))

;; time

(probe :time/stub
  (fn []
    (time)
    ))

;; tstamp

(probe :tstamp/stub
  (fn []
    (tstamp)
    ))

;; fft

(probe :fft/sample
  (fn []
    (fft 0)
    ))

;; ffts

(probe :ffts/spectrum
  (fn []
    (ffts 0)
    ))

;; fftr

(probe :fftr/range
  (fn []
    (fftr 0 100)
    ))

;; fftrs

(probe :fftrs/range
  (fn []
    (fftrs 0 100)
    ))

;; vqt

(probe :vqt/read
  (fn []
    (vqt 0)
    ))

;; vqts

(probe :vqts/read
  (fn []
    (vqts 0)
    ))

;; vqtr

(probe :vqtr/read
  (fn []
    (vqtr 0)
    ))

;; vqtrs

(probe :vqtrs/read
  (fn []
    (vqtrs 0)
    ))

;; vqtw

(probe :vqtw/read
  (fn []
    (vqtw 0)
    ))

;; vqtsw

(probe :vqtsw/read
  (fn []
    (vqtsw 0)
    ))

;; vqtrw

(probe :vqtrw/read
  (fn []
    (vqtrw 0)
    ))

;; vqtrsw

(probe :vqtrsw/read
  (fn []
    (vqtrsw 0)
    ))

;; vbank

(probe :vbank/switch
  (fn []
    (vbank 1)
    (vbank 0)
    ))

(probe :vbank/no-args
  (fn []
    (vbank)
    ))

;; sync

(probe :sync/defaults
  (fn []
    (sync)
    ))

(probe :sync/mask
  (fn []
    (sync 32767 0 false)
    ))

;; exit

(probe :exit/call
  (fn []
    (exit)
    ))

;; reset

(probe :reset/call
  (fn []
    (reset)
    ))

;; The engine runs a cart's TIC every tick; the probes live at the top level.
(global TIC (fn []))
