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

-- The engine runs a cart's TIC every tick; the probes live at the top level.
function TIC() end
