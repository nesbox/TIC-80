# API conformance probes for ruby — the same sections as lua.lua, in the same
# order; that file is the reference and carries the protocol notes.
#
# probe(id, f) traces the marker, then yields; the runner records every API
# call with the values it arrived with, and a call the language refuses ends
# the probe with "!error" — the refusal is a finding of its own.

def probe(id)
  trace('#' + id)

  name = id.split('/')[0]

  unless respond_to?(name, true)
    trace('!missing')
    return
  end

  begin
    yield
  rescue
    trace('!error')
  end
end

# ---- cls

probe('cls/default') do
  pix(10, 10, 4)
  cls()
  pix(10, 10)
end

probe('cls/color') do
  cls(7)
  pix(4, 4)
  pix(235, 131)
end

probe('cls/wrong-type') do
  cls(6)
  cls('x')
  pix(0, 0)
end

probe('cls/out-of-range') do
  pix(0, 0, 6)
  cls(99)
  pix(0, 0)
end

# ---- print

probe('print/width') do
  print('A', 5, 5, 15)
end

probe('print/defaults') do
  print('A')
end

probe('print/scale') do
  print('A', 5, 5, 15, false, 2)
end

probe('print/fixed') do
  print('ii', 0, 0, 15, true)
  print('ww', 0, 10, 15, true)
end

probe('print/no-args') do
  print()
  trace('done')
end

# ---- pix

probe('pix/write') do
  cls(0)
  pix(10, 10, 4)
  pix(10, 10)
end

probe('pix/read') do
  cls(2)
  pix(3, 4)
end

probe('pix/missing-y') do
  cls(5)
  pix(1)
  pix(1, 0)
  pix(0, 0)
end

probe('pix/no-args') do
  cls(3)
  pix()
  pix(0, 0)
end

probe('pix/float-coords') do
  cls(0)
  pix(10.7, 11.7, 4)
  pix(10, 11)
  pix(11, 11)
  pix(10, 12)
end

probe('pix/string-arg') do
  cls(0)
  pix('1', 2, 4)
  pix(1, 2)
end

probe('pix/out-of-bounds') do
  pix(300, 300, 4)
  pix(300, 300)
end

# ---- line

probe('line/diagonal') do
  cls(0)
  line(0, 0, 5, 5, 6)
  pix(0, 0)
  pix(3, 3)
  pix(5, 5)
  pix(5, 0)
end

probe('line/floats') do
  cls(0)
  line(0.4, 1.6, 5.4, 1.6, 6)
  pix(0, 1)
  pix(0, 2)
  pix(5, 1)
  pix(5, 2)
end

probe('line/missing-color') do
  cls(0)
  line(0, 0, 5, 5)
  pix(0, 0)
end

probe('line/one-point') do
  cls(0)
  line(3, 3, 3, 3, 6)
  pix(3, 3)
end

# ---- rect

probe('rect/fill') do
  cls(0)
  rect(1, 1, 3, 3, 6)
  pix(1, 1)
  pix(3, 3)
  pix(4, 4)
  pix(0, 0)
end

probe('rect/zero-size') do
  cls(0)
  rect(5, 5, 0, 0, 6)
  pix(5, 5)
end

probe('rect/negative-size') do
  cls(0)
  rect(5, 5, -2, -2, 6)
  pix(5, 5)
  pix(4, 4)
  pix(3, 3)
end

probe('rect/missing-color') do
  cls(0)
  rect(1, 1, 2, 2)
  pix(1, 1)
end

# ---- rectb

probe('rectb/border') do
  cls(0)
  rectb(1, 1, 4, 4, 6)
  pix(1, 1)
  pix(2, 2)
  pix(4, 4)
  pix(5, 5)
end

probe('rectb/zero-size') do
  cls(0)
  rectb(5, 5, 0, 0, 6)
  pix(5, 5)
end

probe('rectb/one-wide') do
  cls(0)
  rectb(2, 2, 1, 1, 6)
  pix(2, 2)
  pix(3, 3)
end

probe('rectb/missing-color') do
  cls(0)
  rectb(1, 1, 2, 2)
  pix(1, 1)
end

# ---- rrect

probe('rrect/fill') do
  cls(0)
  rrect(10, 10, 20, 20, 6, 4)
  pix(20, 20)
  pix(10, 20)
  pix(14, 10)
  pix(10, 10)
end

probe('rrect/zero-radius') do
  cls(0)
  rrect(10, 10, 20, 20, 6, 0)
  pix(10, 10)
  pix(29, 29)
  pix(30, 30)
end

probe('rrect/exponent') do
  cls(0)
  rrect(10, 10, 20, 20, 6, 4, 4)
  pix(12, 10)
  pix(11, 10)
  pix(10, 11)
  pix(10, 12)
end

probe('rrect/missing-radius') do
  cls(0)
  rrect(10, 10, 20, 20, 6)
  pix(10, 10)
end

# ---- rrectb

probe('rrectb/border') do
  cls(0)
  rrectb(10, 10, 20, 20, 6, 4)
  pix(10, 20)
  pix(20, 10)
  pix(20, 20)
  pix(10, 10)
end

probe('rrectb/zero-radius') do
  cls(0)
  rrectb(10, 10, 20, 20, 6, 0)
  pix(10, 10)
  pix(29, 10)
  pix(20, 20)
end

probe('rrectb/exponent') do
  cls(0)
  rrectb(10, 10, 20, 20, 6, 4, 4)
  pix(11, 10)
  pix(12, 11)
  pix(10, 11)
  pix(12, 10)
end

probe('rrectb/missing-radius') do
  cls(0)
  rrectb(10, 10, 20, 20, 6)
  pix(10, 10)
end

# ---- mget

probe('mget/zero') do
  mget(0, 0)
end

probe('mget/missing-y') do
  mget(3)
end

probe('mget/string-arg') do
  mget('x', 0)
end

# ---- mset

probe('mset/basic') do
  mset(1, 2, 7)
  mget(1, 2)
end

probe('mset/out-of-range-tile') do
  mset(5, 5, 999)
  mget(5, 5)
end

probe('mset/missing-tile') do
  mset(6, 6, 7)
  mset(6, 6)
  mget(6, 6)
end

# ---- peek

probe('peek/zero') do
  poke(8, 0)
  peek(8)
end

probe('peek/missing-addr') do
  peek()
end

probe('peek/out-of-bounds') do
  peek(1000000)
end

probe('peek/negative-addr') do
  peek(-1)
end

# ---- poke

probe('poke/byte') do
  poke(8, 42)
  peek(8)
end

probe('poke/nibble') do
  poke(32, 9, 4)
  peek(32, 4)
end

probe('poke/missing-value') do
  poke(40, 7)
  poke(40)
  peek(40)
end

probe('poke/out-of-bounds') do
  poke(1000000, 1)
end

# ---- trace

probe('trace/basic') do
  trace('hello')
end

probe('trace/color') do
  trace('hello', 5)
end

# ---- spr

probe('spr/basic') do
  cls(0)
  spr(1, 10, 10)
  pix(10, 10)
end

probe('spr/colorkey') do
  cls(0)
  spr(1, 20, 20, 4)
  pix(20, 20)
end

probe('spr/scale') do
  cls(0)
  spr(1, 30, 30, -1, 2)
  pix(30, 30)
end

probe('spr/flip-rotate') do
  cls(0)
  spr(1, 40, 40, -1, 1, 3, 2)
  pix(40, 40)
end

# ---- map

probe('map/defaults') do
  cls(0)
  map()
  pix(0, 0)
end

probe('map/size') do
  cls(0)
  map(0, 0, 4, 4)
  pix(0, 0)
end

probe('map/scale') do
  cls(0)
  map(0, 0, 4, 4, 0, 0, -1, 2)
  pix(0, 0)
end

# ---- btn

probe('btn/pressed') do
  btn(0)
  btn(4)
end

probe('btn/missing-id') do
  btn()
end

# ---- btnp

probe('btnp/pressed') do
  btnp(0)
end

probe('btnp/hold-period') do
  btnp(0, 10, 5)
end

# ---- btnr

probe('btnr/released') do
  btnr(0)
end

probe('btnr/missing-id') do
  btnr()
end

# ---- key

probe('key/no-key') do
  key()
end

probe('key/code') do
  key(1)
end

# ---- keyp

probe('keyp/no-key') do
  keyp()
end

probe('keyp/hold-period') do
  keyp(1, 10, 5)
end

# ---- keyr

probe('keyr/no-key') do
  keyr()
end

probe('keyr/code') do
  keyr(1)
end

# ---- mouse

probe('mouse/idle') do
  mouse()
end

# ---- circ

probe('circ/fill') do
  cls(0)
  circ(20, 20, 5, 6)
  pix(20, 20)
  pix(25, 20)
end

probe('circ/zero-radius') do
  cls(0)
  circ(30, 30, 0, 6)
  pix(30, 30)
end

# ---- circb

probe('circb/border') do
  cls(0)
  circb(20, 20, 5, 6)
  pix(20, 20)
  pix(25, 20)
end

probe('circb/missing-color') do
  cls(0)
  circb(20, 20, 5)
  pix(25, 20)
end

# ---- elli

probe('elli/fill') do
  cls(0)
  elli(40, 40, 6, 3, 6)
  pix(40, 40)
  pix(46, 40)
end

probe('elli/zero-radius') do
  cls(0)
  elli(40, 40, 0, 0, 6)
  pix(40, 40)
end

# ---- ellib

probe('ellib/border') do
  cls(0)
  ellib(40, 40, 6, 3, 6)
  pix(40, 40)
  pix(46, 40)
end

probe('ellib/missing-color') do
  cls(0)
  ellib(40, 40, 6, 3)
  pix(46, 40)
end

# ---- paint

probe('paint/fill') do
  cls(0)
  paint(5, 5, 6)
  pix(0, 0)
  pix(239, 135)
end

probe('paint/missing-color') do
  cls(0)
  paint(5, 5)
  pix(0, 0)
end

# ---- tri

probe('tri/fill') do
  cls(0)
  tri(0, 0, 10, 0, 0, 10, 6)
  pix(0, 0)
  pix(5, 0)
  pix(10, 10)
end

probe('tri/missing-color') do
  cls(0)
  tri(0, 0, 10, 0, 0, 10)
  pix(0, 0)
end

# ---- trib

probe('trib/border') do
  cls(0)
  trib(0, 0, 10, 0, 0, 10, 6)
  pix(0, 0)
  pix(5, 0)
end

probe('trib/missing-color') do
  cls(0)
  trib(0, 0, 10, 0, 0, 10)
  pix(0, 0)
end

# ---- ttri

probe('ttri/basic') do
  cls(0)
  ttri(0, 0, 10, 0, 0, 10, 0, 0, 1, 0, 0, 1)
  pix(0, 0)
  pix(5, 0)
end

probe('ttri/chromakey') do
  cls(0)
  ttri(0, 0, 10, 0, 0, 10, 0, 0, 1, 0, 0, 1, 0, 4)
  pix(0, 0)
end

# ---- clip

probe('clip/set') do
  cls(0)
  clip(2, 2, 5, 5)
  rect(0, 0, 10, 10, 6)
  pix(0, 0)
  pix(2, 2)
  clip()
end

probe('clip/reset') do
  cls(0)
  clip()
  rect(0, 0, 2, 2, 6)
  pix(0, 0)
end

# ---- font

probe('font/text') do
  cls(0)
  font('ab', 0, 0, -1, 6, 6)
  pix(0, 0)
end

# ---- sfx

probe('sfx/basic') do
  sfx(0)
end

probe('sfx/note') do
  sfx(0, 48)
end

probe('sfx/channel') do
  sfx(0, 48, -1, 1)
end

probe('sfx/missing-id') do
  sfx()
end

# ---- music

probe('music/basic') do
  music(0)
end

probe('music/frame') do
  music(0, 0, 0, true, false)
end

# ---- fget

probe('fget/out-of-range') do
  fget(500, 0)
end

# ---- fset

probe('fset/roundtrip') do
  fset(0, 0, true)
  fget(0, 0)
end

probe('fset/missing-bool') do
  fset(0, 1)
  fget(0, 1)
end

# ---- peek1

probe('peek1/read') do
  peek1(16)
end

# ---- poke1

probe('poke1/roundtrip') do
  poke1(16, 7)
  peek1(16)
end

# ---- peek2

probe('peek2/read') do
  peek2(24)
end

# ---- poke2

probe('poke2/roundtrip') do
  poke2(24, 300)
  peek2(24)
end

# ---- peek4

probe('peek4/read') do
  peek4(64)
end

# ---- poke4

probe('poke4/roundtrip') do
  poke4(64, 9)
  peek4(64)
end

# ---- memset

probe('memset/bytes') do
  memset(48, 7, 2)
  peek(48)
  peek(49)
end

# ---- memcpy

probe('memcpy/copy') do
  poke(56, 1)
  poke(57, 2)
  memcpy(60, 56, 2)
  peek(60)
  peek(61)
end

# ---- pmem

probe('pmem/read') do
  pmem(0)
end

probe('pmem/write-read') do
  pmem(0, 42)
  pmem(0)
end

probe('pmem/out-of-range') do
  pmem(1000)
end

# ---- time

probe('time/stub') do
  time()
end

# ---- tstamp

probe('tstamp/stub') do
  tstamp()
end

# ---- fft

probe('fft/sample') do
  fft(0)
end

# ---- ffts

probe('ffts/spectrum') do
  ffts(0)
end

# ---- fftr

probe('fftr/range') do
  fftr(0, 100)
end

# ---- fftrs

probe('fftrs/range') do
  fftrs(0, 100)
end

# ---- vqt

probe('vqt/read') do
  vqt(0)
end

# ---- vqts

probe('vqts/read') do
  vqts(0)
end

# ---- vqtr

probe('vqtr/read') do
  vqtr(0)
end

# ---- vqtrs

probe('vqtrs/read') do
  vqtrs(0)
end

# ---- vqtw

probe('vqtw/read') do
  vqtw(0)
end

# ---- vqtsw

probe('vqtsw/read') do
  vqtsw(0)
end

# ---- vqtrw

probe('vqtrw/read') do
  vqtrw(0)
end

# ---- vqtrsw

probe('vqtrsw/read') do
  vqtrsw(0)
end

# ---- vbank

probe('vbank/switch') do
  vbank(1)
  vbank(0)
end

probe('vbank/no-args') do
  vbank()
  trace('done')
end

# ---- sync

probe('sync/defaults') do
  sync()
end

probe('sync/mask') do
  sync(32767, 0, false)
end

# ---- exit

probe('exit/call') do
  exit()
end

# ---- reset

probe('reset/call') do
  reset()
  trace('done')
end

# The engine runs a cart's TIC every tick; the probes live at the top level.
def TIC
end
