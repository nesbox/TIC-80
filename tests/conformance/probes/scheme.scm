;; API conformance probes for scheme (s7) — the same sections as lua.lua, in
;; the same order; that file is the reference and carries the protocol notes.
;;
;; The api lives under t80::, so every call is (t80::name ...); the prelude
;; traces the marker and runs each body under catch, which with #t takes any
;; error the binding raises. There is no existence check, as in janet: a name
;; the binding lacked would be caught and read as a refusal, not as !missing —
;; no api is missing today.

(define (probe id f)
  (t80::trace (string-append "#" id))
  (catch #t
    (lambda () (f))
    (lambda (tag info) (t80::trace "!error"))))

;; ---- cls

(probe "cls/default" (lambda ()
  (t80::pix 10 10 4)
  (t80::cls)
  (t80::pix 10 10)
  ))

(probe "cls/color" (lambda ()
  (t80::cls 7)
  (t80::pix 4 4)
  (t80::pix 235 131)
  ))

(probe "cls/wrong-type" (lambda ()
  (t80::cls 6)
  (t80::cls "x")
  (t80::pix 0 0)
  ))

(probe "cls/out-of-range" (lambda ()
  (t80::pix 0 0 6)
  (t80::cls 99)
  (t80::pix 0 0)
  ))

;; ---- print

(probe "print/width" (lambda ()
  (t80::print "A" 5 5 15)
  ))

(probe "print/defaults" (lambda ()
  (t80::print "A")
  ))

(probe "print/scale" (lambda ()
  (t80::print "A" 5 5 15 false 2)
  ))

(probe "print/fixed" (lambda ()
  (t80::print "ii" 0 0 15 true)
  (t80::print "ww" 0 10 15 true)
  ))

(probe "print/no-args" (lambda ()
  (t80::print)
  (t80::trace "done")
  ))

;; ---- pix

(probe "pix/write" (lambda ()
  (t80::cls 0)
  (t80::pix 10 10 4)
  (t80::pix 10 10)
  ))

(probe "pix/read" (lambda ()
  (t80::cls 2)
  (t80::pix 3 4)
  ))

(probe "pix/missing-y" (lambda ()
  (t80::cls 5)
  (t80::pix 1)
  (t80::pix 1 0)
  (t80::pix 0 0)
  ))

(probe "pix/no-args" (lambda ()
  (t80::cls 3)
  (t80::pix)
  (t80::pix 0 0)
  ))

(probe "pix/float-coords" (lambda ()
  (t80::cls 0)
  (t80::pix 10.7 11.7 4)
  (t80::pix 10 11)
  (t80::pix 11 11)
  (t80::pix 10 12)
  ))

(probe "pix/string-arg" (lambda ()
  (t80::cls 0)
  (t80::pix "1" 2 4)
  (t80::pix 1 2)
  ))

(probe "pix/out-of-bounds" (lambda ()
  (t80::pix 300 300 4)
  (t80::pix 300 300)
  ))

;; ---- line

(probe "line/diagonal" (lambda ()
  (t80::cls 0)
  (t80::line 0 0 5 5 6)
  (t80::pix 0 0)
  (t80::pix 3 3)
  (t80::pix 5 5)
  (t80::pix 5 0)
  ))

(probe "line/floats" (lambda ()
  (t80::cls 0)
  (t80::line 0.4 1.6 5.4 1.6 6)
  (t80::pix 0 1)
  (t80::pix 0 2)
  (t80::pix 5 1)
  (t80::pix 5 2)
  ))

(probe "line/missing-color" (lambda ()
  (t80::cls 0)
  (t80::line 0 0 5 5)
  (t80::pix 0 0)
  ))

(probe "line/one-point" (lambda ()
  (t80::cls 0)
  (t80::line 3 3 3 3 6)
  (t80::pix 3 3)
  ))

;; ---- rect

(probe "rect/fill" (lambda ()
  (t80::cls 0)
  (t80::rect 1 1 3 3 6)
  (t80::pix 1 1)
  (t80::pix 3 3)
  (t80::pix 4 4)
  (t80::pix 0 0)
  ))

(probe "rect/zero-size" (lambda ()
  (t80::cls 0)
  (t80::rect 5 5 0 0 6)
  (t80::pix 5 5)
  ))

(probe "rect/negative-size" (lambda ()
  (t80::cls 0)
  (t80::rect 5 5 -2 -2 6)
  (t80::pix 5 5)
  (t80::pix 4 4)
  (t80::pix 3 3)
  ))

(probe "rect/missing-color" (lambda ()
  (t80::cls 0)
  (t80::rect 1 1 2 2)
  (t80::pix 1 1)
  ))

;; ---- rectb

(probe "rectb/border" (lambda ()
  (t80::cls 0)
  (t80::rectb 1 1 4 4 6)
  (t80::pix 1 1)
  (t80::pix 2 2)
  (t80::pix 4 4)
  (t80::pix 5 5)
  ))

(probe "rectb/zero-size" (lambda ()
  (t80::cls 0)
  (t80::rectb 5 5 0 0 6)
  (t80::pix 5 5)
  ))

(probe "rectb/one-wide" (lambda ()
  (t80::cls 0)
  (t80::rectb 2 2 1 1 6)
  (t80::pix 2 2)
  (t80::pix 3 3)
  ))

(probe "rectb/missing-color" (lambda ()
  (t80::cls 0)
  (t80::rectb 1 1 2 2)
  (t80::pix 1 1)
  ))

;; ---- rrect

(probe "rrect/fill" (lambda ()
  (t80::cls 0)
  (t80::rrect 10 10 20 20 6 4)
  (t80::pix 20 20)
  (t80::pix 10 20)
  (t80::pix 14 10)
  (t80::pix 10 10)
  ))

(probe "rrect/zero-radius" (lambda ()
  (t80::cls 0)
  (t80::rrect 10 10 20 20 6 0)
  (t80::pix 10 10)
  (t80::pix 29 29)
  (t80::pix 30 30)
  ))

(probe "rrect/exponent" (lambda ()
  (t80::cls 0)
  (t80::rrect 10 10 20 20 6 4 4)
  (t80::pix 12 10)
  (t80::pix 11 10)
  (t80::pix 10 11)
  (t80::pix 10 12)
  ))

(probe "rrect/missing-radius" (lambda ()
  (t80::cls 0)
  (t80::rrect 10 10 20 20 6)
  (t80::pix 10 10)
  ))

;; ---- rrectb

(probe "rrectb/border" (lambda ()
  (t80::cls 0)
  (t80::rrectb 10 10 20 20 6 4)
  (t80::pix 10 20)
  (t80::pix 20 10)
  (t80::pix 20 20)
  (t80::pix 10 10)
  ))

(probe "rrectb/zero-radius" (lambda ()
  (t80::cls 0)
  (t80::rrectb 10 10 20 20 6 0)
  (t80::pix 10 10)
  (t80::pix 29 10)
  (t80::pix 20 20)
  ))

(probe "rrectb/exponent" (lambda ()
  (t80::cls 0)
  (t80::rrectb 10 10 20 20 6 4 4)
  (t80::pix 11 10)
  (t80::pix 12 11)
  (t80::pix 10 11)
  (t80::pix 12 10)
  ))

(probe "rrectb/missing-radius" (lambda ()
  (t80::cls 0)
  (t80::rrectb 10 10 20 20 6)
  (t80::pix 10 10)
  ))

;; ---- mget

(probe "mget/zero" (lambda ()
  (t80::mget 0 0)
  ))

(probe "mget/missing-y" (lambda ()
  (t80::mget 3)
  ))

(probe "mget/string-arg" (lambda ()
  (t80::mget "x" 0)
  ))

;; ---- mset

(probe "mset/basic" (lambda ()
  (t80::mset 1 2 7)
  (t80::mget 1 2)
  ))

(probe "mset/out-of-range-tile" (lambda ()
  (t80::mset 5 5 999)
  (t80::mget 5 5)
  ))

(probe "mset/missing-tile" (lambda ()
  (t80::mset 6 6 7)
  (t80::mset 6 6)
  (t80::mget 6 6)
  ))

;; ---- peek

(probe "peek/zero" (lambda ()
  (t80::poke 8 0)
  (t80::peek 8)
  ))

(probe "peek/missing-addr" (lambda ()
  (t80::peek)
  ))

(probe "peek/out-of-bounds" (lambda ()
  (t80::peek 1000000)
  ))

(probe "peek/negative-addr" (lambda ()
  (t80::peek -1)
  ))

;; ---- poke

(probe "poke/byte" (lambda ()
  (t80::poke 8 42)
  (t80::peek 8)
  ))

(probe "poke/nibble" (lambda ()
  (t80::poke 32 9 4)
  (t80::peek 32 4)
  ))

(probe "poke/missing-value" (lambda ()
  (t80::poke 40 7)
  (t80::poke 40)
  (t80::peek 40)
  ))

(probe "poke/out-of-bounds" (lambda ()
  (t80::poke 1000000 1)
  ))

;; ---- trace

(probe "trace/basic" (lambda ()
  (t80::trace "hello")
  ))

(probe "trace/color" (lambda ()
  (t80::trace "hello" 5)
  ))

;; ---- spr

(probe "spr/basic" (lambda ()
  (t80::cls 0)
  (t80::spr 1 10 10)
  (t80::pix 10 10)
  ))

(probe "spr/colorkey" (lambda ()
  (t80::cls 0)
  (t80::spr 1 20 20 4)
  (t80::pix 20 20)
  ))

(probe "spr/scale" (lambda ()
  (t80::cls 0)
  (t80::spr 1 30 30 -1 2)
  (t80::pix 30 30)
  ))

(probe "spr/flip-rotate" (lambda ()
  (t80::cls 0)
  (t80::spr 1 40 40 -1 1 3 2)
  (t80::pix 40 40)
  ))

;; ---- map

(probe "map/defaults" (lambda ()
  (t80::cls 0)
  (t80::map)
  (t80::pix 0 0)
  ))

(probe "map/size" (lambda ()
  (t80::cls 0)
  (t80::map 0 0 4 4)
  (t80::pix 0 0)
  ))

(probe "map/scale" (lambda ()
  (t80::cls 0)
  (t80::map 0 0 4 4 0 0 -1 2)
  (t80::pix 0 0)
  ))

;; ---- btn

(probe "btn/pressed" (lambda ()
  (t80::btn 0)
  (t80::btn 4)
  ))

(probe "btn/missing-id" (lambda ()
  (t80::btn)
  ))

;; ---- btnp

(probe "btnp/pressed" (lambda ()
  (t80::btnp 0)
  ))

(probe "btnp/hold-period" (lambda ()
  (t80::btnp 0 10 5)
  ))

;; ---- btnr

(probe "btnr/released" (lambda ()
  (t80::btnr 0)
  ))

(probe "btnr/missing-id" (lambda ()
  (t80::btnr)
  ))

;; ---- key

(probe "key/no-key" (lambda ()
  (t80::key)
  ))

(probe "key/code" (lambda ()
  (t80::key 1)
  ))

;; ---- keyp

(probe "keyp/no-key" (lambda ()
  (t80::keyp)
  ))

(probe "keyp/hold-period" (lambda ()
  (t80::keyp 1 10 5)
  ))

;; ---- keyr

(probe "keyr/no-key" (lambda ()
  (t80::keyr)
  ))

(probe "keyr/code" (lambda ()
  (t80::keyr 1)
  ))

;; ---- mouse

(probe "mouse/idle" (lambda ()
  (t80::mouse)
  ))

;; ---- circ

(probe "circ/fill" (lambda ()
  (t80::cls 0)
  (t80::circ 20 20 5 6)
  (t80::pix 20 20)
  (t80::pix 25 20)
  ))

(probe "circ/zero-radius" (lambda ()
  (t80::cls 0)
  (t80::circ 30 30 0 6)
  (t80::pix 30 30)
  ))

;; ---- circb

(probe "circb/border" (lambda ()
  (t80::cls 0)
  (t80::circb 20 20 5 6)
  (t80::pix 20 20)
  (t80::pix 25 20)
  ))

(probe "circb/missing-color" (lambda ()
  (t80::cls 0)
  (t80::circb 20 20 5)
  (t80::pix 25 20)
  ))

;; ---- elli

(probe "elli/fill" (lambda ()
  (t80::cls 0)
  (t80::elli 40 40 6 3 6)
  (t80::pix 40 40)
  (t80::pix 46 40)
  ))

(probe "elli/zero-radius" (lambda ()
  (t80::cls 0)
  (t80::elli 40 40 0 0 6)
  (t80::pix 40 40)
  ))

;; ---- ellib

(probe "ellib/border" (lambda ()
  (t80::cls 0)
  (t80::ellib 40 40 6 3 6)
  (t80::pix 40 40)
  (t80::pix 46 40)
  ))

(probe "ellib/missing-color" (lambda ()
  (t80::cls 0)
  (t80::ellib 40 40 6 3)
  (t80::pix 46 40)
  ))

;; ---- paint

(probe "paint/fill" (lambda ()
  (t80::cls 0)
  (t80::paint 5 5 6)
  (t80::pix 0 0)
  (t80::pix 239 135)
  ))

(probe "paint/missing-color" (lambda ()
  (t80::cls 0)
  (t80::paint 5 5)
  (t80::pix 0 0)
  ))

;; ---- tri

(probe "tri/fill" (lambda ()
  (t80::cls 0)
  (t80::tri 0 0 10 0 0 10 6)
  (t80::pix 0 0)
  (t80::pix 5 0)
  (t80::pix 10 10)
  ))

(probe "tri/missing-color" (lambda ()
  (t80::cls 0)
  (t80::tri 0 0 10 0 0 10)
  (t80::pix 0 0)
  ))

;; ---- trib

(probe "trib/border" (lambda ()
  (t80::cls 0)
  (t80::trib 0 0 10 0 0 10 6)
  (t80::pix 0 0)
  (t80::pix 5 0)
  ))

(probe "trib/missing-color" (lambda ()
  (t80::cls 0)
  (t80::trib 0 0 10 0 0 10)
  (t80::pix 0 0)
  ))

;; ---- ttri

(probe "ttri/basic" (lambda ()
  (t80::cls 0)
  (t80::ttri 0 0 10 0 0 10 0 0 1 0 0 1)
  (t80::pix 0 0)
  (t80::pix 5 0)
  ))

(probe "ttri/chromakey" (lambda ()
  (t80::cls 0)
  (t80::ttri 0 0 10 0 0 10 0 0 1 0 0 1 0 4)
  (t80::pix 0 0)
  ))

;; ---- clip

(probe "clip/set" (lambda ()
  (t80::cls 0)
  (t80::clip 2 2 5 5)
  (t80::rect 0 0 10 10 6)
  (t80::pix 0 0)
  (t80::pix 2 2)
  (t80::clip)
  ))

(probe "clip/reset" (lambda ()
  (t80::cls 0)
  (t80::clip)
  (t80::rect 0 0 2 2 6)
  (t80::pix 0 0)
  ))

;; ---- font

(probe "font/text" (lambda ()
  (t80::cls 0)
  (t80::font "ab" 0 0 -1 6 6)
  (t80::pix 0 0)
  ))

;; ---- sfx

(probe "sfx/basic" (lambda ()
  (t80::sfx 0)
  ))

(probe "sfx/note" (lambda ()
  (t80::sfx 0 48)
  ))

(probe "sfx/channel" (lambda ()
  (t80::sfx 0 48 -1 1)
  ))

(probe "sfx/missing-id" (lambda ()
  (t80::sfx)
  ))

;; ---- music

(probe "music/basic" (lambda ()
  (t80::music 0)
  ))

(probe "music/frame" (lambda ()
  (t80::music 0 0 0 true false)
  ))

;; ---- fget

(probe "fget/out-of-range" (lambda ()
  (t80::fget 500 0)
  ))

;; ---- fset

(probe "fset/roundtrip" (lambda ()
  (t80::fset 0 0 true)
  (t80::fget 0 0)
  ))

(probe "fset/missing-bool" (lambda ()
  (t80::fset 0 1)
  (t80::fget 0 1)
  ))

;; ---- peek1

(probe "peek1/read" (lambda ()
  (t80::peek1 16)
  ))

;; ---- poke1

(probe "poke1/roundtrip" (lambda ()
  (t80::poke1 16 7)
  (t80::peek1 16)
  ))

;; ---- peek2

(probe "peek2/read" (lambda ()
  (t80::peek2 24)
  ))

;; ---- poke2

(probe "poke2/roundtrip" (lambda ()
  (t80::poke2 24 300)
  (t80::peek2 24)
  ))

;; ---- peek4

(probe "peek4/read" (lambda ()
  (t80::peek4 64)
  ))

;; ---- poke4

(probe "poke4/roundtrip" (lambda ()
  (t80::poke4 64 9)
  (t80::peek4 64)
  ))

;; ---- memset

(probe "memset/bytes" (lambda ()
  (t80::memset 48 7 2)
  (t80::peek 48)
  (t80::peek 49)
  ))

;; ---- memcpy

(probe "memcpy/copy" (lambda ()
  (t80::poke 56 1)
  (t80::poke 57 2)
  (t80::memcpy 60 56 2)
  (t80::peek 60)
  (t80::peek 61)
  ))

;; ---- pmem

(probe "pmem/read" (lambda ()
  (t80::pmem 0)
  ))

(probe "pmem/write-read" (lambda ()
  (t80::pmem 0 42)
  (t80::pmem 0)
  ))

(probe "pmem/out-of-range" (lambda ()
  (t80::pmem 1000)
  ))

;; ---- time

(probe "time/stub" (lambda ()
  (t80::time)
  ))

;; ---- tstamp

(probe "tstamp/stub" (lambda ()
  (t80::tstamp)
  ))

;; ---- fft

(probe "fft/sample" (lambda ()
  (t80::fft 0)
  ))

;; ---- ffts

(probe "ffts/spectrum" (lambda ()
  (t80::ffts 0)
  ))

;; ---- fftr

(probe "fftr/range" (lambda ()
  (t80::fftr 0 100)
  ))

;; ---- fftrs

(probe "fftrs/range" (lambda ()
  (t80::fftrs 0 100)
  ))

;; ---- vqt

(probe "vqt/read" (lambda ()
  (t80::vqt 0)
  ))

;; ---- vqts

(probe "vqts/read" (lambda ()
  (t80::vqts 0)
  ))

;; ---- vqtr

(probe "vqtr/read" (lambda ()
  (t80::vqtr 0)
  ))

;; ---- vqtrs

(probe "vqtrs/read" (lambda ()
  (t80::vqtrs 0)
  ))

;; ---- vqtw

(probe "vqtw/read" (lambda ()
  (t80::vqtw 0)
  ))

;; ---- vqtsw

(probe "vqtsw/read" (lambda ()
  (t80::vqtsw 0)
  ))

;; ---- vqtrw

(probe "vqtrw/read" (lambda ()
  (t80::vqtrw 0)
  ))

;; ---- vqtrsw

(probe "vqtrsw/read" (lambda ()
  (t80::vqtrsw 0)
  ))

;; ---- vbank

(probe "vbank/switch" (lambda ()
  (t80::vbank 1)
  (t80::vbank 0)
  ))

(probe "vbank/no-args" (lambda ()
  (t80::vbank)
  (t80::trace "done")
  ))

;; ---- sync

(probe "sync/defaults" (lambda ()
  (t80::sync)
  ))

(probe "sync/mask" (lambda ()
  (t80::sync 32767 0 false)
  ))

;; ---- exit

(probe "exit/call" (lambda ()
  (t80::exit)
  ))

;; ---- reset

(probe "reset/call" (lambda ()
  (t80::reset)
  (t80::trace "done")
  ))

;; The engine runs a cart's TIC every tick; the probes live at the top level.
(define (TIC) #t)
