// API conformance probes for squirrel — the same sections as lua.lua, in the
// same order; that file is the reference and carries the protocol notes.
//
// probe(id, f) traces the marker, then runs f. The runner records every API
// call with the values it arrived with; a call the language refuses ends the
// probe with "!error" — the refusal is a finding of its own. Every literal is
// double-quoted: squirrel's 'c' is a character constant, and 'ab' does not
// compile.

function probe(id, f) {
    trace("#" + id);

    local name = id.slice(0, id.find("/"));

    if (typeof getroottable().rawget(name) != "function") {
        trace("!missing");
        return;
    }

    try { f(); }
    catch (e) { trace("!error"); }
}

// ---- cls

probe("cls/default", function() {
    pix(10, 10, 4);
    cls();
    pix(10, 10);
});

probe("cls/color", function() {
    cls(7);
    pix(4, 4);
    pix(235, 131);
});

probe("cls/wrong-type", function() {
    cls(6);
    cls("x");
    pix(0, 0);
});

probe("cls/out-of-range", function() {
    pix(0, 0, 6);
    cls(99);
    pix(0, 0);
});

// ---- print

probe("print/width", function() {
    print("A", 5, 5, 15);
});

probe("print/defaults", function() {
    print("A");
});

probe("print/scale", function() {
    print("A", 5, 5, 15, false, 2);
});

probe("print/fixed", function() {
    print("ii", 0, 0, 15, true);
    print("ww", 0, 10, 15, true);
});

probe("print/no-args", function() {
    print();
    trace("done");
});

// ---- pix

probe("pix/write", function() {
    cls(0);
    pix(10, 10, 4);
    pix(10, 10);
});

probe("pix/read", function() {
    cls(2);
    pix(3, 4);
});

probe("pix/missing-y", function() {
    cls(5);
    pix(1);
    pix(1, 0);
    pix(0, 0);
});

probe("pix/no-args", function() {
    cls(3);
    pix();
    pix(0, 0);
});

probe("pix/float-coords", function() {
    cls(0);
    pix(10.7, 11.7, 4);
    pix(10, 11);
    pix(11, 11);
    pix(10, 12);
});

probe("pix/string-arg", function() {
    cls(0);
    pix("1", 2, 4);
    pix(1, 2);
});

probe("pix/out-of-bounds", function() {
    pix(300, 300, 4);
    pix(300, 300);
});

// ---- line

probe("line/diagonal", function() {
    cls(0);
    line(0, 0, 5, 5, 6);
    pix(0, 0);
    pix(3, 3);
    pix(5, 5);
    pix(5, 0);
});

probe("line/floats", function() {
    cls(0);
    line(0.4, 1.6, 5.4, 1.6, 6);
    pix(0, 1);
    pix(0, 2);
    pix(5, 1);
    pix(5, 2);
});

probe("line/missing-color", function() {
    cls(0);
    line(0, 0, 5, 5);
    pix(0, 0);
});

probe("line/one-point", function() {
    cls(0);
    line(3, 3, 3, 3, 6);
    pix(3, 3);
});

// ---- rect

probe("rect/fill", function() {
    cls(0);
    rect(1, 1, 3, 3, 6);
    pix(1, 1);
    pix(3, 3);
    pix(4, 4);
    pix(0, 0);
});

probe("rect/zero-size", function() {
    cls(0);
    rect(5, 5, 0, 0, 6);
    pix(5, 5);
});

probe("rect/negative-size", function() {
    cls(0);
    rect(5, 5, -2, -2, 6);
    pix(5, 5);
    pix(4, 4);
    pix(3, 3);
});

probe("rect/missing-color", function() {
    cls(0);
    rect(1, 1, 2, 2);
    pix(1, 1);
});

// ---- rectb

probe("rectb/border", function() {
    cls(0);
    rectb(1, 1, 4, 4, 6);
    pix(1, 1);
    pix(2, 2);
    pix(4, 4);
    pix(5, 5);
});

probe("rectb/zero-size", function() {
    cls(0);
    rectb(5, 5, 0, 0, 6);
    pix(5, 5);
});

probe("rectb/one-wide", function() {
    cls(0);
    rectb(2, 2, 1, 1, 6);
    pix(2, 2);
    pix(3, 3);
});

probe("rectb/missing-color", function() {
    cls(0);
    rectb(1, 1, 2, 2);
    pix(1, 1);
});

// ---- rrect

probe("rrect/fill", function() {
    cls(0);
    rrect(10, 10, 20, 20, 6, 4);
    pix(20, 20);
    pix(10, 20);
    pix(14, 10);
    pix(10, 10);
});

probe("rrect/zero-radius", function() {
    cls(0);
    rrect(10, 10, 20, 20, 6, 0);
    pix(10, 10);
    pix(29, 29);
    pix(30, 30);
});

probe("rrect/exponent", function() {
    cls(0);
    rrect(10, 10, 20, 20, 6, 4, 4);
    pix(12, 10);
    pix(11, 10);
    pix(10, 11);
    pix(10, 12);
});

probe("rrect/missing-radius", function() {
    cls(0);
    rrect(10, 10, 20, 20, 6);
    pix(10, 10);
});

// ---- rrectb

probe("rrectb/border", function() {
    cls(0);
    rrectb(10, 10, 20, 20, 6, 4);
    pix(10, 20);
    pix(20, 10);
    pix(20, 20);
    pix(10, 10);
});

probe("rrectb/zero-radius", function() {
    cls(0);
    rrectb(10, 10, 20, 20, 6, 0);
    pix(10, 10);
    pix(29, 10);
    pix(20, 20);
});

probe("rrectb/exponent", function() {
    cls(0);
    rrectb(10, 10, 20, 20, 6, 4, 4);
    pix(11, 10);
    pix(12, 11);
    pix(10, 11);
    pix(12, 10);
});

probe("rrectb/missing-radius", function() {
    cls(0);
    rrectb(10, 10, 20, 20, 6);
    pix(10, 10);
});

// ---- mget

probe("mget/zero", function() {
    mget(0, 0);
});

probe("mget/missing-y", function() {
    mget(3);
});

probe("mget/string-arg", function() {
    mget("x", 0);
});

// ---- mset

probe("mset/basic", function() {
    mset(1, 2, 7);
    mget(1, 2);
});

probe("mset/out-of-range-tile", function() {
    mset(5, 5, 999);
    mget(5, 5);
});

probe("mset/missing-tile", function() {
    mset(6, 6, 7);
    mset(6, 6);
    mget(6, 6);
});

// ---- peek

probe("peek/zero", function() {
    poke(8, 0);
    peek(8);
});

probe("peek/missing-addr", function() {
    peek();
});

probe("peek/out-of-bounds", function() {
    peek(1000000);
});

probe("peek/negative-addr", function() {
    peek(-1);
});

// ---- poke

probe("poke/byte", function() {
    poke(8, 42);
    peek(8);
});

probe("poke/nibble", function() {
    poke(32, 9, 4);
    peek(32, 4);
});

probe("poke/missing-value", function() {
    poke(40, 7);
    poke(40);
    peek(40);
});

probe("poke/out-of-bounds", function() {
    poke(1000000, 1);
});

// ---- trace

probe("trace/basic", function() {
    trace("hello");
});

probe("trace/color", function() {
    trace("hello", 5);
});

// ---- spr

probe("spr/basic", function() {
    cls(0);
    spr(1, 10, 10);
    pix(10, 10);
});

probe("spr/colorkey", function() {
    cls(0);
    spr(1, 20, 20, 4);
    pix(20, 20);
});

probe("spr/scale", function() {
    cls(0);
    spr(1, 30, 30, -1, 2);
    pix(30, 30);
});

probe("spr/flip-rotate", function() {
    cls(0);
    spr(1, 40, 40, -1, 1, 3, 2);
    pix(40, 40);
});

// ---- map

probe("map/defaults", function() {
    cls(0);
    map();
    pix(0, 0);
});

probe("map/size", function() {
    cls(0);
    map(0, 0, 4, 4);
    pix(0, 0);
});

probe("map/scale", function() {
    cls(0);
    map(0, 0, 4, 4, 0, 0, -1, 2);
    pix(0, 0);
});

// ---- btn

probe("btn/pressed", function() {
    btn(0);
    btn(4);
});

probe("btn/missing-id", function() {
    btn();
});

// ---- btnp

probe("btnp/pressed", function() {
    btnp(0);
});

probe("btnp/hold-period", function() {
    btnp(0, 10, 5);
});

// ---- key

probe("key/no-key", function() {
    key();
});

probe("key/code", function() {
    key(1);
});

// ---- keyp

probe("keyp/no-key", function() {
    keyp();
});

probe("keyp/hold-period", function() {
    keyp(1, 10, 5);
});

// ---- mouse

probe("mouse/idle", function() {
    mouse();
});

// ---- circ

probe("circ/fill", function() {
    cls(0);
    circ(20, 20, 5, 6);
    pix(20, 20);
    pix(25, 20);
});

probe("circ/zero-radius", function() {
    cls(0);
    circ(30, 30, 0, 6);
    pix(30, 30);
});

// ---- circb

probe("circb/border", function() {
    cls(0);
    circb(20, 20, 5, 6);
    pix(20, 20);
    pix(25, 20);
});

probe("circb/missing-color", function() {
    cls(0);
    circb(20, 20, 5);
    pix(25, 20);
});

// ---- elli

probe("elli/fill", function() {
    cls(0);
    elli(40, 40, 6, 3, 6);
    pix(40, 40);
    pix(46, 40);
});

probe("elli/zero-radius", function() {
    cls(0);
    elli(40, 40, 0, 0, 6);
    pix(40, 40);
});

// ---- ellib

probe("ellib/border", function() {
    cls(0);
    ellib(40, 40, 6, 3, 6);
    pix(40, 40);
    pix(46, 40);
});

probe("ellib/missing-color", function() {
    cls(0);
    ellib(40, 40, 6, 3);
    pix(46, 40);
});

// ---- paint

probe("paint/fill", function() {
    cls(0);
    paint(5, 5, 6);
    pix(0, 0);
    pix(239, 135);
});

probe("paint/missing-color", function() {
    cls(0);
    paint(5, 5);
    pix(0, 0);
});

// ---- tri

probe("tri/fill", function() {
    cls(0);
    tri(0, 0, 10, 0, 0, 10, 6);
    pix(0, 0);
    pix(5, 0);
    pix(10, 10);
});

probe("tri/missing-color", function() {
    cls(0);
    tri(0, 0, 10, 0, 0, 10);
    pix(0, 0);
});

// ---- trib

probe("trib/border", function() {
    cls(0);
    trib(0, 0, 10, 0, 0, 10, 6);
    pix(0, 0);
    pix(5, 0);
});

probe("trib/missing-color", function() {
    cls(0);
    trib(0, 0, 10, 0, 0, 10);
    pix(0, 0);
});

// ---- ttri

probe("ttri/basic", function() {
    cls(0);
    ttri(0, 0, 10, 0, 0, 10, 0, 0, 1, 0, 0, 1);
    pix(0, 0);
    pix(5, 0);
});

probe("ttri/chromakey", function() {
    cls(0);
    ttri(0, 0, 10, 0, 0, 10, 0, 0, 1, 0, 0, 1, 0, 4);
    pix(0, 0);
});

// ---- clip

probe("clip/set", function() {
    cls(0);
    clip(2, 2, 5, 5);
    rect(0, 0, 10, 10, 6);
    pix(0, 0);
    pix(2, 2);
    clip();
});

probe("clip/reset", function() {
    cls(0);
    clip();
    rect(0, 0, 2, 2, 6);
    pix(0, 0);
});

// ---- font

probe("font/text", function() {
    cls(0);
    font("ab", 0, 0, -1, 6, 6);
    pix(0, 0);
});

// ---- sfx

probe("sfx/basic", function() {
    sfx(0);
});

probe("sfx/note", function() {
    sfx(0, 48);
});

probe("sfx/channel", function() {
    sfx(0, 48, -1, 1);
});

probe("sfx/missing-id", function() {
    sfx();
});

// ---- music

probe("music/basic", function() {
    music(0);
});

probe("music/frame", function() {
    music(0, 0, 0, true, false);
});

// ---- fget

probe("fget/out-of-range", function() {
    fget(500, 0);
});

// ---- fset

probe("fset/roundtrip", function() {
    fset(0, 0, true);
    fget(0, 0);
});

probe("fset/missing-bool", function() {
    fset(0, 1);
    fget(0, 1);
});

// ---- peek1

probe("peek1/read", function() {
    peek1(16);
});

// ---- poke1

probe("poke1/roundtrip", function() {
    poke1(16, 7);
    peek1(16);
});

// ---- peek2

probe("peek2/read", function() {
    peek2(24);
});

// ---- poke2

probe("poke2/roundtrip", function() {
    poke2(24, 300);
    peek2(24);
});

// ---- peek4

probe("peek4/read", function() {
    peek4(64);
});

// ---- poke4

probe("poke4/roundtrip", function() {
    poke4(64, 9);
    peek4(64);
});

// ---- memset

probe("memset/bytes", function() {
    memset(48, 7, 2);
    peek(48);
    peek(49);
});

// ---- memcpy

probe("memcpy/copy", function() {
    poke(56, 1);
    poke(57, 2);
    memcpy(60, 56, 2);
    peek(60);
    peek(61);
});

// ---- pmem

probe("pmem/read", function() {
    pmem(0);
});

probe("pmem/write-read", function() {
    pmem(0, 42);
    pmem(0);
});

probe("pmem/out-of-range", function() {
    pmem(1000);
});

// ---- time

probe("time/stub", function() {
    time();
});

// ---- tstamp

probe("tstamp/stub", function() {
    tstamp();
});

// ---- fft

probe("fft/sample", function() {
    fft(0);
});

// ---- ffts

probe("ffts/spectrum", function() {
    ffts(0);
});

// ---- fftr

probe("fftr/range", function() {
    fftr(0, 100);
});

// ---- fftrs

probe("fftrs/range", function() {
    fftrs(0, 100);
});

// ---- vqt

probe("vqt/read", function() {
    vqt(0);
});

// ---- vqts

probe("vqts/read", function() {
    vqts(0);
});

// ---- vqtr

probe("vqtr/read", function() {
    vqtr(0);
});

// ---- vqtrs

probe("vqtrs/read", function() {
    vqtrs(0);
});

// ---- vqtw

probe("vqtw/read", function() {
    vqtw(0);
});

// ---- vqtsw

probe("vqtsw/read", function() {
    vqtsw(0);
});

// ---- vqtrw

probe("vqtrw/read", function() {
    vqtrw(0);
});

// ---- vqtrsw

probe("vqtrsw/read", function() {
    vqtrsw(0);
});

// ---- vbank

probe("vbank/switch", function() {
    vbank(1);
    vbank(0);
});

probe("vbank/no-args", function() {
    vbank();
    trace("done");
});

// ---- sync

probe("sync/defaults", function() {
    sync();
});

probe("sync/mask", function() {
    sync(32767, 0, false);
});

// ---- exit

probe("exit/call", function() {
    exit();
});

// ---- reset

probe("reset/call", function() {
    reset();
    trace("done");
});

// The engine runs a cart's TIC every tick; the probes live at the top level.
function TIC() {}
