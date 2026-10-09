-- API conformance probes for lua — the reference every other language is
-- diffed against, and the full list of probes: a language's probe file covers
-- the same sections in the same order.
--
-- probe(id, f) traces the marker "#api/variant", then runs f. The runner
-- records every API call with the values it arrived with and its return, so
-- a probe is just a sequence of calls: what one call does is visible in the
-- calls that follow it. A call the language refuses ends the probe with
-- "!error" — the refusal is a finding of its own.
--
-- A probe reads only state it wrote, and touches no time, rnd or input.

local function probe(id, f)
    trace('#' .. id)

    if type(_G[id:match('^([^/]+)')]) ~= 'function' then
        trace('!missing')
        return
    end

    if not pcall(f) then trace('!error') end
end

---- cls

probe('cls/default', function()
    pix(10, 10, 4)
    cls()
    pix(10, 10)
end)

probe('cls/color', function()
    cls(7)
    pix(4, 4)
    pix(235, 131)
end)

probe('cls/wrong-type', function()
    cls(6)
    cls('x')
    pix(0, 0)
end)

probe('cls/out-of-range', function()
    pix(0, 0, 6)
    cls(99)
    pix(0, 0)
end)

---- print

probe('print/width', function()
    print('A', 5, 5, 15)
end)

probe('print/defaults', function()
    print('A')
end)

probe('print/scale', function()
    print('A', 5, 5, 15, false, 2)
end)

probe('print/fixed', function()
    print('ii', 0, 0, 15, true)
    print('ww', 0, 10, 15, true)
end)

probe('print/no-args', function()
    print()
    trace('done')
end)

---- pix

probe('pix/write', function()
    cls(0)
    pix(10, 10, 4)
    pix(10, 10)
end)

probe('pix/read', function()
    cls(2)
    pix(3, 4)
end)

probe('pix/missing-y', function()
    cls(5)
    pix(1)
    pix(1, 0)
    pix(0, 0)
end)

probe('pix/no-args', function()
    cls(3)
    pix()
    pix(0, 0)
end)

probe('pix/float-coords', function()
    cls(0)
    pix(10.7, 11.7, 4)
    pix(10, 11)
    pix(11, 11)
    pix(10, 12)
end)

probe('pix/string-arg', function()
    cls(0)
    pix('1', 2, 4)
    pix(1, 2)
end)

probe('pix/out-of-bounds', function()
    pix(300, 300, 4)
    pix(300, 300)
end)

---- line

probe('line/diagonal', function()
    cls(0)
    line(0, 0, 5, 5, 6)
    pix(0, 0)
    pix(3, 3)
    pix(5, 5)
    pix(5, 0)
end)

probe('line/floats', function()
    cls(0)
    line(0.4, 1.6, 5.4, 1.6, 6)
    pix(0, 1)
    pix(0, 2)
    pix(5, 1)
    pix(5, 2)
end)

probe('line/missing-color', function()
    cls(0)
    line(0, 0, 5, 5)
    pix(0, 0)
end)

probe('line/one-point', function()
    cls(0)
    line(3, 3, 3, 3, 6)
    pix(3, 3)
end)

---- rect

probe('rect/fill', function()
    cls(0)
    rect(1, 1, 3, 3, 6)
    pix(1, 1)
    pix(3, 3)
    pix(4, 4)
    pix(0, 0)
end)

probe('rect/zero-size', function()
    cls(0)
    rect(5, 5, 0, 0, 6)
    pix(5, 5)
end)

probe('rect/negative-size', function()
    cls(0)
    rect(5, 5, -2, -2, 6)
    pix(5, 5)
    pix(4, 4)
    pix(3, 3)
end)

probe('rect/missing-color', function()
    cls(0)
    rect(1, 1, 2, 2)
    pix(1, 1)
end)

---- rectb

probe('rectb/border', function()
    cls(0)
    rectb(1, 1, 4, 4, 6)
    pix(1, 1)
    pix(2, 2)
    pix(4, 4)
    pix(5, 5)
end)

probe('rectb/zero-size', function()
    cls(0)
    rectb(5, 5, 0, 0, 6)
    pix(5, 5)
end)

probe('rectb/one-wide', function()
    cls(0)
    rectb(2, 2, 1, 1, 6)
    pix(2, 2)
    pix(3, 3)
end)

probe('rectb/missing-color', function()
    cls(0)
    rectb(1, 1, 2, 2)
    pix(1, 1)
end)

---- mget

probe('mget/zero', function()
    mget(0, 0)
end)

probe('mget/missing-y', function()
    mget(3)
end)

probe('mget/string-arg', function()
    mget('x', 0)
end)

---- mset

probe('mset/basic', function()
    mset(1, 2, 7)
    mget(1, 2)
end)

probe('mset/out-of-range-tile', function()
    mset(5, 5, 999)
    mget(5, 5)
end)

probe('mset/missing-tile', function()
    mset(6, 6, 7)
    mset(6, 6)
    mget(6, 6)
end)

---- peek

probe('peek/zero', function()
    poke(8, 0)
    peek(8)
end)

probe('peek/missing-addr', function()
    peek()
end)

probe('peek/out-of-bounds', function()
    peek(1000000)
end)

probe('peek/negative-addr', function()
    peek(-1)
end)

---- poke

probe('poke/byte', function()
    poke(8, 42)
    peek(8)
end)

probe('poke/nibble', function()
    poke(32, 9, 4)
    peek(32, 4)
end)

probe('poke/missing-value', function()
    poke(40, 7)
    poke(40)
    peek(40)
end)

probe('poke/out-of-bounds', function()
    poke(1000000, 1)
end)

---- trace

probe('trace/basic', function()
    trace('hello')
end)

probe('trace/color', function()
    trace('hello', 5)
end)

---- spr

probe('spr/basic', function()
    cls(0)
    spr(1, 10, 10)
    pix(10, 10)
end)

probe('spr/colorkey', function()
    cls(0)
    spr(1, 20, 20, 4)
    pix(20, 20)
end)

probe('spr/scale', function()
    cls(0)
    spr(1, 30, 30, -1, 2)
    pix(30, 30)
end)

probe('spr/flip-rotate', function()
    cls(0)
    spr(1, 40, 40, -1, 1, 3, 2)
    pix(40, 40)
end)

---- map

probe('map/defaults', function()
    cls(0)
    map()
    pix(0, 0)
end)

probe('map/size', function()
    cls(0)
    map(0, 0, 4, 4)
    pix(0, 0)
end)

probe('map/scale', function()
    cls(0)
    map(0, 0, 4, 4, 0, 0, -1, 2)
    pix(0, 0)
end)

---- btn

probe('btn/pressed', function()
    btn(0)
    btn(4)
end)

probe('btn/missing-id', function()
    btn()
end)

---- btnp

probe('btnp/pressed', function()
    btnp(0)
end)

probe('btnp/hold-period', function()
    btnp(0, 10, 5)
end)

---- key

probe('key/no-key', function()
    key()
end)

probe('key/code', function()
    key(1)
end)

---- keyp

probe('keyp/no-key', function()
    keyp()
end)

probe('keyp/hold-period', function()
    keyp(1, 10, 5)
end)

---- mouse

probe('mouse/idle', function()
    mouse()
end)

---- circ

probe('circ/fill', function()
    cls(0)
    circ(20, 20, 5, 6)
    pix(20, 20)
    pix(25, 20)
end)

probe('circ/zero-radius', function()
    cls(0)
    circ(30, 30, 0, 6)
    pix(30, 30)
end)

---- circb

probe('circb/border', function()
    cls(0)
    circb(20, 20, 5, 6)
    pix(20, 20)
    pix(25, 20)
end)

probe('circb/missing-color', function()
    cls(0)
    circb(20, 20, 5)
    pix(25, 20)
end)

---- elli

probe('elli/fill', function()
    cls(0)
    elli(40, 40, 6, 3, 6)
    pix(40, 40)
    pix(46, 40)
end)

probe('elli/zero-radius', function()
    cls(0)
    elli(40, 40, 0, 0, 6)
    pix(40, 40)
end)

---- ellib

probe('ellib/border', function()
    cls(0)
    ellib(40, 40, 6, 3, 6)
    pix(40, 40)
    pix(46, 40)
end)

probe('ellib/missing-color', function()
    cls(0)
    ellib(40, 40, 6, 3)
    pix(46, 40)
end)

---- paint

probe('paint/fill', function()
    cls(0)
    paint(5, 5, 6)
    pix(0, 0)
    pix(239, 135)
end)

probe('paint/missing-color', function()
    cls(0)
    paint(5, 5)
    pix(0, 0)
end)

---- tri

probe('tri/fill', function()
    cls(0)
    tri(0, 0, 10, 0, 0, 10, 6)
    pix(0, 0)
    pix(5, 0)
    pix(10, 10)
end)

probe('tri/missing-color', function()
    cls(0)
    tri(0, 0, 10, 0, 0, 10)
    pix(0, 0)
end)

---- trib

probe('trib/border', function()
    cls(0)
    trib(0, 0, 10, 0, 0, 10, 6)
    pix(0, 0)
    pix(5, 0)
end)

probe('trib/missing-color', function()
    cls(0)
    trib(0, 0, 10, 0, 0, 10)
    pix(0, 0)
end)

---- ttri

probe('ttri/basic', function()
    cls(0)
    ttri(0, 0, 10, 0, 0, 10, 0, 0, 1, 0, 0, 1)
    pix(0, 0)
    pix(5, 0)
end)

probe('ttri/chromakey', function()
    cls(0)
    ttri(0, 0, 10, 0, 0, 10, 0, 0, 1, 0, 0, 1, 0, 4)
    pix(0, 0)
end)

---- clip

probe('clip/set', function()
    cls(0)
    clip(2, 2, 5, 5)
    rect(0, 0, 10, 10, 6)
    pix(0, 0)
    pix(2, 2)
    clip()
end)

probe('clip/reset', function()
    cls(0)
    clip()
    rect(0, 0, 2, 2, 6)
    pix(0, 0)
end)

---- font

probe('font/text', function()
    cls(0)
    font('ab', 0, 0, -1, 6, 6)
    pix(0, 0)
end)

---- sfx

probe('sfx/basic', function()
    sfx(0)
end)

probe('sfx/note', function()
    sfx(0, 48)
end)

probe('sfx/channel', function()
    sfx(0, 48, -1, 1)
end)

probe('sfx/missing-id', function()
    sfx()
end)

---- music

probe('music/basic', function()
    music(0)
end)

probe('music/frame', function()
    music(0, 0, 0, true, false)
end)

---- fget

probe('fget/out-of-range', function()
    fget(500, 0)
end)

---- fset

probe('fset/roundtrip', function()
    fset(0, 0, true)
    fget(0, 0)
end)

probe('fset/missing-bool', function()
    fset(0, 1)
    fget(0, 1)
end)

---- peek1

probe('peek1/read', function()
    peek1(16)
end)

---- poke1

probe('poke1/roundtrip', function()
    poke1(16, 7)
    peek1(16)
end)

---- peek2

probe('peek2/read', function()
    peek2(24)
end)

---- poke2

probe('poke2/roundtrip', function()
    poke2(24, 300)
    peek2(24)
end)

---- peek4

probe('peek4/read', function()
    peek4(64)
end)

---- poke4

probe('poke4/roundtrip', function()
    poke4(64, 9)
    peek4(64)
end)

---- memset

probe('memset/bytes', function()
    memset(48, 7, 2)
    peek(48)
    peek(49)
end)

---- memcpy

probe('memcpy/copy', function()
    poke(56, 1)
    poke(57, 2)
    memcpy(60, 56, 2)
    peek(60)
    peek(61)
end)

---- pmem

probe('pmem/read', function()
    pmem(0)
end)

probe('pmem/write-read', function()
    pmem(0, 42)
    pmem(0)
end)

probe('pmem/out-of-range', function()
    pmem(1000)
end)

---- time

probe('time/stub', function()
    time()
end)

---- tstamp

probe('tstamp/stub', function()
    tstamp()
end)

---- fft

probe('fft/sample', function()
    fft(0)
end)

---- ffts

probe('ffts/spectrum', function()
    ffts(0)
end)

---- fftr

probe('fftr/range', function()
    fftr(0, 100)
end)

---- fftrs

probe('fftrs/range', function()
    fftrs(0, 100)
end)

---- vqt

probe('vqt/read', function()
    vqt(0)
end)

---- vqts

probe('vqts/read', function()
    vqts(0)
end)

---- vqtr

probe('vqtr/read', function()
    vqtr(0)
end)

---- vqtrs

probe('vqtrs/read', function()
    vqtrs(0)
end)

---- vqtw

probe('vqtw/read', function()
    vqtw(0)
end)

---- vqtsw

probe('vqtsw/read', function()
    vqtsw(0)
end)

---- vqtrw

probe('vqtrw/read', function()
    vqtrw(0)
end)

---- vqtrsw

probe('vqtrsw/read', function()
    vqtrsw(0)
end)

---- vbank

probe('vbank/switch', function()
    vbank(1)
    vbank(0)
end)

probe('vbank/no-args', function()
    vbank()
    trace('done')
end)

---- sync

probe('sync/defaults', function()
    sync()
end)

probe('sync/mask', function()
    sync(32767, 0, false)
end)

---- exit

probe('exit/call', function()
    exit()
end)

---- reset

probe('reset/call', function()
    reset()
    trace('done')
end)

-- The engine runs a cart's TIC every tick; the probes live at the top level.
function TIC() end
