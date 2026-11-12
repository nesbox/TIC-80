-- title:  pattern
-- author: Nesbox
-- desc:   fill primitives with two colours and a 4x4 tile
-- script: lua

local function f(c1, c2, p) return c1 + c2*256 + p*65536 end

local ramp =
{
    0x0001, 0x0401, 0x0405, 0x0505, 0x0525,
    0x8525, 0x85A5, 0xA5A5, 0xA5A7, 0xADA7,
    0xADAF, 0xAFAF, 0xAFBF, 0xEFBF, 0xEFFF,
}

local motifs =
{
    0xA5A5, -- checkerboard
    0x5555, -- vertical stripes
    0xAAAA, -- vertical stripes, other phase
    0x0F0F, -- horizontal stripes
    0xF0F0, -- horizontal stripes, other phase
    0x8421, -- diagonal
    0x1248, -- other diagonal
    0x0001, -- one pixel in sixteen
}

function TIC()
    cls(0)

    print("patterned fills: dither ramp", 3, 3, 12)

    -- 15 ordered-dither levels, colour 6 over colour 8.
    for i = 1, 15 do
        rect(3 + (i - 1) * 15, 12, 13, 13, f(6, 8, ramp[i]))
    end

    -- The named tiles, colour 12 over colour 1.
    for i = 1, 8 do
        rect(3 + (i - 1) * 29, 30, 26, 13, f(12, 1, motifs[i]))
    end

    -- Borders, a circle, a triangle and a line take the same word.
    rectb(3, 49, 60, 34, f(11, 3, 0xA5A5))
    circ(100, 66, 16, f(9, 13, 0x5555))
    circb(140, 66, 16, f(12, 5, 0x0F0F))
    tri(160, 49, 200, 49, 180, 83, f(14, 4, 0x8421))
    line(3, 89, 236, 89, f(10, 6, 0xF0F0))

    -- A transparent second colour stipples one colour over a live background.
    for i = 0, 15 do
        rect(i * 15, 95, 15, 30, i)
    end

    rect(12, 98, 216, 24, f(15, 255, 0x8421))
    print("c2 = 255 leaves the screen", 14, 127, 12)
end

-- <PALETTE>
-- 000:1a1c2c5d275db13e53ef7d57ffcd75a7f07038b76425717929366f3b5dc941a6f673eff7f4f4f494b0c2566c86333c57
-- </PALETTE>
