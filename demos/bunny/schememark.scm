;; title: Bunnymark in Scheme
;; author: David St-Hilaire, based on the original work of Rob Loach
;; desc: Benchmarking tool to see how many bunnies can fly around the screen, using Scheme.
;; license: MIT License 
;; input: gamepad
;; script: scheme
;; version: 1.1.0

(define screenWidth 240)
(define screenHeight 136)
(define toolbarHeight 6)
(define t 0)

(define (randomFloat lower greater) (+ (random (- greater lower) lower)))

(defstruct bunny 
  (w 26)
  (h 32)
  (x (floor (random (- screenWidth 26))))
  (y (floor (random (- screenHeight 32))))
  (vx (/ (- (random 200) 100) 60))
  (vy (/ (- (random 200) 100) 60))
  (sprite 1))

(define (bunny-draw b)
  (t80::spr (bunny-sprite b) (floor (bunny-x b)) (floor (bunny-y b)) 1 1 0 0 4 4))

(define (bunny-update b)
  (bunny-set-x! b (+ (bunny-x b) (bunny-vx b)))
  (bunny-set-y! b (+ (bunny-y b) (bunny-vy b)))

  (if (or (> (+ (bunny-x b) (bunny-w b)) screenWidth)
          (< (bunny-x b) 0))
      (bunny-set-vx! b (* (bunny-vx b) -1)))

  (if (or (> (+ (bunny-y b) (bunny-h b)) screenHeight)
          (< (bunny-y b) toolbarHeight))
      (bunny-set-vy! b (* (bunny-vy b) -1))))

(define fps-value 0)
(define fps-frames 0)
(define fps-lastTime 0)

(define (fps-getValue)
  (let ((current-time (t80::time)))
    (if (<= (- current-time fps-lastTime)
            1000)
        (set! fps-frames (+ fps-frames 1))
        (begin (set! fps-value fps-frames)
               (set! fps-frames 0)
               (set! fps-lastTime current-time))))
  fps-value)

(define (drop lst n) (let loop ((l (length lst)) (lst lst) (newlst '()))
                       (if (<= l n)
                           (reverse newlst)
                           (loop (- l 1) (cdr lst) (cons (car lst) newlst)))))

(define bunnies (vector (make-bunny)))

(define (TIC)
  ;; music
  (if (= t 0) (t80::music 0))
  (if (= t (* 6 64 2.375)) (t80::music 1))
  (set! t (+ t 1))

  ;; input
  (if (t80::btn 0)
      (let loop ((i 1) (new-bunnies '()))
        (if (<= i 5)
            (loop (+ i 1) (cons (make-bunny) new-bunnies))
            (set! bunnies (list->vector (append new-bunnies (vector->list bunnies)))))))

  (if (t80::btn 1)
      (set! bunnies (list->vector (drop (vector->list bunnies) 5))))

  ;; update
  (for-each (lambda (b) (bunny-update b))
            bunnies)

  ;; draw
  (t80::cls 15)
  (for-each (lambda (b) (bunny-draw b))
            bunnies)

  (t80::rect 0 0 screenWidth toolbarHeight 0)
  (t80::print (format #t "Bunnies: ~d\n" (length bunnies)) 1 0 11 #f 1 #f)
  (t80::print (format #t "FPS: ~f\n" (fps-getValue)) (/ screenWidth 2) 0 11 #f 1 #f))

;; <TILES>
;; 001:11111100111110dd111110dc111110dc111110dc111110dc111110dd111110dd
;; 002:00011110ddd0110dccd0110dccd0110dccd0110dccd0110dcddd00dddddddddd
;; 003:00001111dddd0111cccd0111cccd0111cccd0111cccd0111dcdd0111dddd0111
;; 004:1111111111111111111111111111111111111111111111111111111111111111
;; 017:111110dd111110dd111110dd111110dd10000ddd1eeeeddd1eeeeedd10000eed
;; 018:d0ddddddd0ddddddddddddddddd0000dddddccddddddccdddddddddddddddddd
;; 019:0ddd01110ddd0111dddd0111dddd0111ddddd000ddddddddddddddddddddd000
;; 020:1111111111111111111111111111111101111111d0111111d011111101111111
;; 033:111110ee111110ee111110ee111110ee111110ee111110ee111110ee111110ee
;; 034:dddcccccddccccccddccccccddccccccddccccccdddcccccdddddddddddddddd
;; 035:dddd0111cddd0111cddd0111cddd0111cddd0111dddd0111dddd0111dddd0111
;; 036:1111111111111111111111111111111111111111111111111111111111111111
;; 049:111110ee111110ee111110ee111110ee111110ee111110ee111110ee11111100
;; 050:dddeeeeeddeeeeeed00000000111111101111111011111110111111111111111
;; 051:eddd0111eedd01110eed011110ee011110ee011110ee011110ee011111001111
;; 052:1111111111111111111111111111111111111111111111111111111111111111
;; </TILES>

;; <PALETTE>
;; 000:1a1c2c5d275db13e53ef7d57ffcd75a7f07038b76425717929366f3b5dc941a6f673eff7f4f4f494b0c2566c86333c57
;; </PALETTE>
