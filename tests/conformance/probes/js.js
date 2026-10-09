// API conformance probes for js — the same sections as lua.lua, in the same
// order; that file is the reference and carries the protocol notes.
//
// probe(id, f) traces the marker, then runs f. The runner records every API
// call with the values it arrived with; a call the language refuses ends the
// probe with "!error" — the refusal is a finding of its own.

function probe(id, f) {
    trace('#' + id);

    if (typeof globalThis[id.split('/')[0]] !== 'function') {
        trace('!missing');
        return;
    }

    try { f(); }
    catch (e) { trace('!error'); }
}

// ---- cls

probe('cls/default', () => {
    pix(10, 10, 4);
    cls();
    pix(10, 10);
});

probe('cls/color', () => {
    cls(7);
    pix(4, 4);
    pix(235, 131);
});

probe('cls/wrong-type', () => {
    cls(6);
    cls('x');
    pix(0, 0);
});

probe('cls/out-of-range', () => {
    pix(0, 0, 6);
    cls(99);
    pix(0, 0);
});

// ---- print

probe('print/width', () => {
    print('A', 5, 5, 15);
});

probe('print/defaults', () => {
    print('A');
});

probe('print/scale', () => {
    print('A', 5, 5, 15, false, 2);
});

probe('print/fixed', () => {
    print('ii', 0, 0, 15, true);
    print('ww', 0, 10, 15, true);
});

probe('print/no-args', () => {
    print();
});

// ---- pix

probe('pix/write', () => {
    cls(0);
    pix(10, 10, 4);
    pix(10, 10);
});

probe('pix/read', () => {
    cls(2);
    pix(3, 4);
});

probe('pix/missing-y', () => {
    cls(5);
    pix(1);
    pix(1, 0);
    pix(0, 0);
});

probe('pix/no-args', () => {
    cls(3);
    pix();
    pix(0, 0);
});

probe('pix/float-coords', () => {
    cls(0);
    pix(10.7, 11.7, 4);
    pix(10, 11);
    pix(11, 11);
    pix(10, 12);
});

probe('pix/string-arg', () => {
    cls(0);
    pix('1', 2, 4);
    pix(1, 2);
});

probe('pix/out-of-bounds', () => {
    pix(300, 300, 4);
    pix(300, 300);
});

// ---- line

probe('line/diagonal', () => {
    cls(0);
    line(0, 0, 5, 5, 6);
    pix(0, 0);
    pix(3, 3);
    pix(5, 5);
    pix(5, 0);
});

probe('line/floats', () => {
    cls(0);
    line(0.4, 1.6, 5.4, 1.6, 6);
    pix(0, 1);
    pix(0, 2);
    pix(5, 1);
    pix(5, 2);
});

probe('line/missing-color', () => {
    cls(0);
    line(0, 0, 5, 5);
    pix(0, 0);
});

probe('line/one-point', () => {
    cls(0);
    line(3, 3, 3, 3, 6);
    pix(3, 3);
});

// ---- rect

probe('rect/fill', () => {
    cls(0);
    rect(1, 1, 3, 3, 6);
    pix(1, 1);
    pix(3, 3);
    pix(4, 4);
    pix(0, 0);
});

probe('rect/zero-size', () => {
    cls(0);
    rect(5, 5, 0, 0, 6);
    pix(5, 5);
});

probe('rect/negative-size', () => {
    cls(0);
    rect(5, 5, -2, -2, 6);
    pix(5, 5);
    pix(4, 4);
    pix(3, 3);
});

probe('rect/missing-color', () => {
    cls(0);
    rect(1, 1, 2, 2);
    pix(1, 1);
});

// ---- rectb

probe('rectb/border', () => {
    cls(0);
    rectb(1, 1, 4, 4, 6);
    pix(1, 1);
    pix(2, 2);
    pix(4, 4);
    pix(5, 5);
});

probe('rectb/zero-size', () => {
    cls(0);
    rectb(5, 5, 0, 0, 6);
    pix(5, 5);
});

probe('rectb/one-wide', () => {
    cls(0);
    rectb(2, 2, 1, 1, 6);
    pix(2, 2);
    pix(3, 3);
});

probe('rectb/missing-color', () => {
    cls(0);
    rectb(1, 1, 2, 2);
    pix(1, 1);
});

// ---- mget

probe('mget/zero', () => {
    mget(0, 0);
});

probe('mget/missing-y', () => {
    mget(3);
});

probe('mget/string-arg', () => {
    mget('x', 0);
});

// ---- mset

probe('mset/basic', () => {
    mset(1, 2, 7);
    mget(1, 2);
});

probe('mset/out-of-range-tile', () => {
    mset(5, 5, 999);
    mget(5, 5);
});

probe('mset/missing-tile', () => {
    mset(6, 6, 7);
    mset(6, 6);
    mget(6, 6);
});

// ---- peek

probe('peek/zero', () => {
    poke(8, 0);
    peek(8);
});

probe('peek/missing-addr', () => {
    peek();
});

probe('peek/out-of-bounds', () => {
    peek(1000000);
});

probe('peek/negative-addr', () => {
    peek(-1);
});

// ---- poke

probe('poke/byte', () => {
    poke(8, 42);
    peek(8);
});

probe('poke/nibble', () => {
    poke(32, 9, 4);
    peek(32, 4);
});

probe('poke/missing-value', () => {
    poke(40, 7);
    poke(40);
    peek(40);
});

probe('poke/out-of-bounds', () => {
    poke(1000000, 1);
});

// The engine runs a cart's TIC every tick; the probes live at the top level.
function TIC() {}
