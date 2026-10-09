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
    (pix 10 10)))

(probe :cls/color
  (fn []
    (cls 7)
    (pix 4 4)
    (pix 235 131)))

(probe :cls/wrong-type
  (fn []
    (cls 6)
    (cls "x")
    (pix 0 0)))

(probe :cls/out-of-range
  (fn []
    (pix 0 0 6)
    (cls 99)
    (pix 0 0)))

;; print

(probe :print/width
  (fn []
    (print "A" 5 5 15)))

(probe :print/defaults
  (fn []
    (print "A")))

(probe :print/scale
  (fn []
    (print "A" 5 5 15 false 2)))

(probe :print/fixed
  (fn []
    (print "ii" 0 0 15 true)
    (print "ww" 0 10 15 true)))

(probe :print/no-args
  (fn []
    (print)))

;; pix

(probe :pix/write
  (fn []
    (cls 0)
    (pix 10 10 4)
    (pix 10 10)))

(probe :pix/read
  (fn []
    (cls 2)
    (pix 3 4)))

(probe :pix/missing-y
  (fn []
    (cls 5)
    (pix 1)
    (pix 1 0)
    (pix 0 0)))

(probe :pix/no-args
  (fn []
    (cls 3)
    (pix)
    (pix 0 0)))

(probe :pix/float-coords
  (fn []
    (cls 0)
    (pix 10.7 11.7 4)
    (pix 10 11)
    (pix 11 11)
    (pix 10 12)))

(probe :pix/string-arg
  (fn []
    (cls 0)
    (pix "1" 2 4)
    (pix 1 2)))

(probe :pix/out-of-bounds
  (fn []
    (pix 300 300 4)
    (pix 300 300)))

;; line

(probe :line/diagonal
  (fn []
    (cls 0)
    (line 0 0 5 5 6)
    (pix 0 0)
    (pix 3 3)
    (pix 5 5)
    (pix 5 0)))

(probe :line/floats
  (fn []
    (cls 0)
    (line 0.4 1.6 5.4 1.6 6)
    (pix 0 1)
    (pix 0 2)
    (pix 5 1)
    (pix 5 2)))

(probe :line/missing-color
  (fn []
    (cls 0)
    (line 0 0 5 5)
    (pix 0 0)))

(probe :line/one-point
  (fn []
    (cls 0)
    (line 3 3 3 3 6)
    (pix 3 3)))

;; rect

(probe :rect/fill
  (fn []
    (cls 0)
    (rect 1 1 3 3 6)
    (pix 1 1)
    (pix 3 3)
    (pix 4 4)
    (pix 0 0)))

(probe :rect/zero-size
  (fn []
    (cls 0)
    (rect 5 5 0 0 6)
    (pix 5 5)))

(probe :rect/negative-size
  (fn []
    (cls 0)
    (rect 5 5 -2 -2 6)
    (pix 5 5)
    (pix 4 4)
    (pix 3 3)))

(probe :rect/missing-color
  (fn []
    (cls 0)
    (rect 1 1 2 2)
    (pix 1 1)))

;; rectb

(probe :rectb/border
  (fn []
    (cls 0)
    (rectb 1 1 4 4 6)
    (pix 1 1)
    (pix 2 2)
    (pix 4 4)
    (pix 5 5)))

(probe :rectb/zero-size
  (fn []
    (cls 0)
    (rectb 5 5 0 0 6)
    (pix 5 5)))

(probe :rectb/one-wide
  (fn []
    (cls 0)
    (rectb 2 2 1 1 6)
    (pix 2 2)
    (pix 3 3)))

(probe :rectb/missing-color
  (fn []
    (cls 0)
    (rectb 1 1 2 2)
    (pix 1 1)))

;; mget

(probe :mget/zero
  (fn []
    (mget 0 0)))

(probe :mget/missing-y
  (fn []
    (mget 3)))

(probe :mget/string-arg
  (fn []
    (mget "x" 0)))

;; mset

(probe :mset/basic
  (fn []
    (mset 1 2 7)
    (mget 1 2)))

(probe :mset/out-of-range-tile
  (fn []
    (mset 5 5 999)
    (mget 5 5)))

(probe :mset/missing-tile
  (fn []
    (mset 6 6 7)
    (mset 6 6)
    (mget 6 6)))

;; peek

(probe :peek/zero
  (fn []
    (poke 8 0)
    (peek 8)))

(probe :peek/missing-addr
  (fn []
    (peek)))

(probe :peek/out-of-bounds
  (fn []
    (peek 1000000)))

(probe :peek/negative-addr
  (fn []
    (peek -1)))

;; poke

(probe :poke/byte
  (fn []
    (poke 8 42)
    (peek 8)))

(probe :poke/nibble
  (fn []
    (poke 32 9 4)
    (peek 32 4)))

(probe :poke/missing-value
  (fn []
    (poke 40 7)
    (poke 40)
    (peek 40)))

(probe :poke/out-of-bounds
  (fn []
    (poke 1000000 1)))

;; The engine runs a cart's TIC every tick; the probes live at the top level.
(global TIC (fn []))
