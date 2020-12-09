;; title:  game title
;; author: game developer
;; desc:   short description
;; script: wasm

(module
  (type (;0;) (func))
  (type (;1;) (func (param i32)))
  (type (;2;) (func (result i32)))
  (type (;3;) (func (param i32) (result i32)))
  (type (;4;) (func (param i32 i32 i32 i32 i32 i32 i32 i32 i32)))
  (type (;5;) (func (param i32 i32 i32 i32 i32 i32 i32) (result i32)))
  (import "env" "btn" (func (;0;) (type 3)))
  (import "env" "cls" (func (;1;) (type 1)))
  (import "env" "spr" (func (;2;) (type 4)))
  (import "env" "print" (func (;3;) (type 5)))
  (func (;4;) (type 0)
    nop)
  (func (;5;) (type 0)
    i32.const 0
    call 0
    if  ;; label = @1
      i32.const 1044
      i32.const 1044
      i32.load
      i32.const 1
      i32.sub
      i32.store
    end
    i32.const 1
    call 0
    if  ;; label = @1
      i32.const 1044
      i32.const 1044
      i32.load
      i32.const 1
      i32.add
      i32.store
    end
    i32.const 2
    call 0
    if  ;; label = @1
      i32.const 1040
      i32.const 1040
      i32.load
      i32.const 1
      i32.sub
      i32.store
    end
    i32.const 3
    call 0
    if  ;; label = @1
      i32.const 1040
      i32.const 1040
      i32.load
      i32.const 1
      i32.add
      i32.store
    end
    i32.const 13
    call 1
    i32.const 1048
    i32.load
    i32.const 60
    i32.rem_s
    i32.const 24
    i32.shl
    i32.const 24
    i32.shr_s
    i32.const 30
    i32.div_s
    i32.const 24
    i32.shl
    i32.const 24
    i32.shr_s
    i32.const 1
    i32.shl
    i32.const 1
    i32.or
    i32.const 1040
    i32.load
    i32.const 1044
    i32.load
    i32.const 14
    i32.const 3
    i32.const 0
    i32.const 0
    i32.const 2
    i32.const 2
    call 2
    i32.const 1024
    i32.const 84
    i32.const 84
    i32.const 15
    i32.const 0
    i32.const 1
    i32.const 0
    call 3
    drop
    i32.const 1048
    i32.const 1048
    i32.load
    i32.const 1
    i32.add
    i32.store)
  (func (;6;) (type 2) (result i32)
    global.get 0)
  (func (;7;) (type 1) (param i32)
    local.get 0
    global.set 0)
  (func (;8;) (type 3) (param i32) (result i32)
    global.get 0
    local.get 0
    i32.sub
    i32.const -16
    i32.and
    local.tee 0
    global.set 0
    local.get 0)
  (func (;9;) (type 2) (result i32)
    i32.const 1052)
  (table (;0;) 2 2 funcref)
  (memory (;0;) 256 256)
  (global (;0;) (mut i32) (i32.const 5243936))
  (export "memory" (memory 0))
  (export "__indirect_function_table" (table 0))
  (export "TIC" (func 5))
  (export "_initialize" (func 4))
  (export "__errno_location" (func 9))
  (export "stackSave" (func 6))
  (export "stackRestore" (func 7))
  (export "stackAlloc" (func 8))
  (elem (;0;) (i32.const 1) func 4)
  (data (;0;) (i32.const 1024) "HELLO WORLD!")
  (data (;1;) (i32.const 1040) "`\00\00\00\18"))

;; <TILES>
;; 001:eccccccccc888888caaaaaaaca888888cacccccccacc0ccccacc0ccccacc0ccc
;; 002:ccccceee8888cceeaaaa0cee888a0ceeccca0ccc0cca0c0c0cca0c0c0cca0c0c
;; 003:eccccccccc888888caaaaaaaca888888cacccccccacccccccacc0ccccacc0ccc
;; 004:ccccceee8888cceeaaaa0cee888a0ceeccca0cccccca0c0c0cca0c0c0cca0c0c
;; 017:cacccccccaaaaaaacaaacaaacaaaaccccaaaaaaac8888888cc000cccecccccec
;; 018:ccca00ccaaaa0ccecaaa0ceeaaaa0ceeaaaa0cee8888ccee000cceeecccceeee
;; 019:cacccccccaaaaaaacaaacaaacaaaaccccaaaaaaac8888888cc000cccecccccec
;; 020:ccca00ccaaaa0ccecaaa0ceeaaaa0ceeaaaa0cee8888ccee000cceeecccceeee
;; </TILES>

;; <WAVES>
;; 000:00000000ffffffff00000000ffffffff
;; 001:0123456789abcdeffedcba9876543210
;; 002:0123456789abcdef0123456789abcdef
;; </WAVES>

;; <SFX>
;; 000:000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000304000000000
;; </SFX>

;; <PALETTE>
;; 000:1a1c2c5d275db13e53ef7d57ffcd75a7f07038b76425717929366f3b5dc941a6f673eff7f4f4f494b0c2566c86333c57
;; </PALETTE>

