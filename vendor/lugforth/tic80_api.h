// Copyright 2026 Luginfo10
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

// The words of the TIC-80 API, shared by the module (ports/tic80/builtins.h)
// and by the host (src/api/lugforth.c in TIC-80, which keeps a copy of this
// file): API(id, name, op, arguments, results). The id is the first argument
// of the import tic80.api; the names and stack effects are those of the pForth
// binding TIC-80 had before (src/api/forth.c), so its cartridges keep working.
// EXITGAME and KEYPRESSED are renamed so as not to hide EXIT and KEY.
//
// Stack effects, arguments in order, the last one on top:
//   CLS ( color -- )                         PRINT ( a n x y color fixed scale alt -- width )
//   PIX ( x y -- color )                     PIX! ( x y color -- )
//   LINE ( x0 y0 x1 y1 color -- )            RECT / RECTB ( x y w h color -- )
//   SPR ( id x y colorkey scale flip rotate w h -- )   colorkey -1: none
//   BTN / BTNR ( id -- f )                   BTNP ( id hold period -- f )
//   SFX ( id note octave duration channel volume speed -- )
//   MAP ( cellx celly cellw cellh sx sy colorkey scale -- )
//   MGET ( x y -- tile )                     MSET ( x y tile -- )
//   PEEK ( addr bits -- n )                  POKE ( addr n bits -- )
//   PEEK1 / PEEK2 / PEEK4 ( addr -- n )      POKE1 / POKE2 / POKE4 ( addr n -- )
//   MEMCPY ( dst src size -- )               MEMSET ( dst value size -- )
//   TRACE ( a n color -- )                   PMEM ( index -- n )   PMEM! ( n index -- )
//   TIME ( -- ms )   TSTAMP ( -- s )         EXITGAME ( -- )
//   FONT ( a n x y chromakey cw ch fixed scale alt -- width )
//   MOUSE ( -- x y left middle right scrollx scrolly )
//   CIRC / CIRCB ( x y r color -- )          ELLI / ELLIB ( x y a b color -- )
//   PAINT ( x y color border -- )            TRI / TRIB ( x1 y1 x2 y2 x3 y3 color -- )
//   TTRI ( x1 y1 x2 y2 x3 y3 u1 v1 u2 v2 u3 v3 texsrc colorkey z1 z2 z3 depth -- )
//   CLIP ( x y w h -- )   CLIP0 ( -- )       MUSIC ( track frame row loop sustain tempo speed -- )
//   SYNC ( mask bank tocart -- )             VBANK ( bank -- previous )   RESET ( -- )
//   KEYPRESSED / KEYR ( code -- f )          KEYP ( code hold period -- f )
//   FGET ( id flag -- f )   FSET ( id flag f -- )
//   FFT / FFTS ( start end -- n )   n: 0..65535
//   RRECT / RRECTB ( x y w h color radius exponent -- )

#define TIC80_API_LIST \
  API(0, "CLS", CLS, 1, 0) \
  API(1, "PRINT", PRINT, 8, 1) \
  API(2, "PIX", PIX, 2, 1) \
  API(3, "PIX!", PIX_STORE, 3, 0) \
  API(4, "LINE", LINE, 5, 0) \
  API(5, "RECT", RECT, 5, 0) \
  API(6, "RECTB", RECTB, 5, 0) \
  API(7, "SPR", SPR, 9, 0) \
  API(8, "BTN", BTN, 1, 1) \
  API(9, "BTNP", BTNP, 3, 1) \
  API(10, "SFX", SFX, 7, 0) \
  API(11, "MAP", MAP, 8, 0) \
  API(12, "MGET", MGET, 2, 1) \
  API(13, "MSET", MSET, 3, 0) \
  API(14, "PEEK", PEEK, 2, 1) \
  API(15, "POKE", POKE, 3, 0) \
  API(16, "PEEK1", PEEK1, 1, 1) \
  API(17, "POKE1", POKE1, 2, 0) \
  API(18, "PEEK2", PEEK2, 1, 1) \
  API(19, "POKE2", POKE2, 2, 0) \
  API(20, "PEEK4", PEEK4, 1, 1) \
  API(21, "POKE4", POKE4, 2, 0) \
  API(22, "MEMCPY", MEMCPY, 3, 0) \
  API(23, "MEMSET", MEMSET, 3, 0) \
  API(24, "TRACE", TRACE, 3, 0) \
  API(25, "PMEM", PMEM, 1, 1) \
  API(26, "PMEM!", PMEM_STORE, 2, 0) \
  API(27, "TIME", TIME, 0, 1) \
  API(28, "TSTAMP", TSTAMP, 0, 1) \
  API(29, "EXITGAME", EXITGAME, 0, 0) \
  API(30, "FONT", FONT, 10, 1) \
  API(31, "MOUSE", MOUSE, 0, 7) \
  API(32, "CIRC", CIRC, 4, 0) \
  API(33, "CIRCB", CIRCB, 4, 0) \
  API(34, "ELLI", ELLI, 5, 0) \
  API(35, "ELLIB", ELLIB, 5, 0) \
  API(36, "PAINT", PAINT, 4, 0) \
  API(37, "TRI", TRI, 7, 0) \
  API(38, "TRIB", TRIB, 7, 0) \
  API(39, "TTRI", TTRI, 18, 0) \
  API(40, "CLIP", CLIP, 4, 0) \
  API(41, "CLIP0", CLIP0, 0, 0) \
  API(42, "MUSIC", MUSIC, 7, 0) \
  API(43, "SYNC", SYNC, 3, 0) \
  API(44, "VBANK", VBANK, 1, 1) \
  API(45, "RESET", RESET, 0, 0) \
  API(46, "KEYPRESSED", KEYPRESSED, 1, 1) \
  API(47, "KEYP", KEYP, 3, 1) \
  API(48, "FGET", FGET, 2, 1) \
  API(49, "FSET", FSET, 3, 0) \
  API(50, "FFT", FFT, 2, 1) \
  API(51, "FFTS", FFTS, 2, 1) \
  API(52, "RRECT", RRECT, 7, 0) \
  API(53, "RRECTB", RRECTB, 7, 0) \
  API(54, "BTNR", BTNR, 1, 1) \
  API(55, "KEYR", KEYR, 1, 1)
