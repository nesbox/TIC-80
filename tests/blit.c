/* The blit contract: tic_core_blit_ex composes vbank0 and vbank1 into the
 * whole 256x144 product screen, draws the border rows and refreshes the
 * palettes around the callbacks. The reference below is the per-pixel
 * definition of that picture; every case compares the whole frame. */
#include "core/core.h"
#include "api.h"
#include "tools.h"

#include <assert.h>
#include <stdio.h>
#include <string.h>

#define FULL_PIXELS (TIC80_FULLWIDTH * TIC80_FULLHEIGHT)

static tic_mem* Tic;
static u32 actual[FULL_PIXELS];
static u32 reference[FULL_PIXELS];
static tic_vram savedMem, savedRam;
static s32 savedId;

static tic_core* core(void)
{
    return (tic_core*)Tic;
}

static tic_vram* vbank0(void)
{
    return core()->state.vbank.id ? &core()->state.vbank.mem : &core()->memory.ram->vram;
}

static tic_vram* vbank1(void)
{
    return core()->state.vbank.id ? &core()->memory.ram->vram : &core()->state.vbank.mem;
}

static void refUpdpal(tic_blitpal* pal0, tic_blitpal* pal1)
{
    *pal0 = tic_tool_palette_blit(&vbank0()->palette, core()->screen_format);
    *pal1 = tic_tool_palette_blit(&vbank1()->palette, core()->screen_format);
}

static void refUpdbdr(s32 row, u32* ptr, tic_blit_callback clb, tic_blitpal* pal0, tic_blitpal* pal1)
{
    if(clb.border) clb.border(Tic, row, clb.data);

    if(clb.scanline)
    {
        if(row == 0)
            clb.scanline(Tic, 0, clb.data);
        else if(row > TIC80_MARGIN_TOP && row < (TIC80_HEIGHT + TIC80_MARGIN_TOP))
            clb.scanline(Tic, row - TIC80_MARGIN_TOP, clb.data);
    }

    if(clb.border || clb.scanline) refUpdpal(pal0, pal1);

    for(s32 i = 0; i < TIC80_FULLWIDTH; i++)
        ptr[i] = pal0->data[vbank0()->vars.border];
}

static u32 refBlitpix(s32 offset0, s32 offset1, const tic_blitpal* pal0, const tic_blitpal* pal1)
{
    u8 pix = tic_tool_peek4(vbank1()->screen.data, offset1);

    return pix != vbank1()->vars.clear ? pal1->data[pix] : pal0->data[tic_tool_peek4(vbank0()->screen.data, offset0)];
}

static void refBlit(tic_blit_callback clb)
{
    tic_blitpal pal0, pal1;
    refUpdpal(&pal0, &pal1);

    s32 row = 0;
    u32* rowPtr = reference;

    for(; row != TIC80_MARGIN_TOP; ++row, rowPtr += TIC80_FULLWIDTH)
        refUpdbdr(row, rowPtr, clb, &pal0, &pal1);

    for(; row != TIC80_FULLHEIGHT - TIC80_MARGIN_BOTTOM; ++row)
    {
        refUpdbdr(row, rowPtr, clb, &pal0, &pal1);
        rowPtr += TIC80_MARGIN_LEFT;

        if(vbank0()->vars.offset.x == 0 && vbank0()->vars.offset.y == 0 && vbank1()->vars.offset.x == 0 &&
           vbank1()->vars.offset.y == 0)
        {
            for(s32 x = (row - TIC80_MARGIN_TOP) * TIC80_WIDTH, end = x + TIC80_WIDTH; x != end; ++x)
                *rowPtr++ = refBlitpix(x, x, &pal0, &pal1);
        }
        else
        {
            enum
            {
                OffsetY = TIC80_HEIGHT - TIC80_MARGIN_TOP
            };
            s32 start0 = (row + vbank0()->vars.offset.y + OffsetY) % TIC80_HEIGHT * TIC80_WIDTH;
            s32 start1 = (row + vbank1()->vars.offset.y + OffsetY) % TIC80_HEIGHT * TIC80_WIDTH;
            s32 offsetX0 = vbank0()->vars.offset.x;
            s32 offsetX1 = vbank1()->vars.offset.x;

            for(s32 x = TIC80_WIDTH; x != 2 * TIC80_WIDTH; ++x)
                *rowPtr++ = refBlitpix((x + offsetX0) % TIC80_WIDTH + start0,
                                       (x + offsetX1) % TIC80_WIDTH + start1, &pal0, &pal1);
        }

        rowPtr += TIC80_MARGIN_RIGHT;
    }

    for(; row != TIC80_FULLHEIGHT; ++row, rowPtr += TIC80_FULLWIDTH)
        refUpdbdr(row, rowPtr, clb, &pal0, &pal1);
}

#define MAX_BORDER_ROWS TIC80_FULLHEIGHT
#define MAX_SCAN_ROWS   TIC80_HEIGHT

static s32 borderLog[MAX_BORDER_ROWS], borderCount;
static s32 scanLog[MAX_SCAN_ROWS], scanCount;
static s32 callbackMarker;

static void recordBorder(tic_mem* tic, s32 row, void* data)
{
    (void)tic;
    (void)data;

    assert(tic == Tic);
    assert(data == &callbackMarker);
    assert(borderCount < MAX_BORDER_ROWS);

    borderLog[borderCount++] = row;
}

static void recordScanline(tic_mem* tic, s32 row, void* data)
{
    (void)tic;
    (void)data;

    assert(tic == Tic);
    assert(data == &callbackMarker);
    assert(scanCount < MAX_SCAN_ROWS);

    scanLog[scanCount++] = row;
}

// The callbacks own the palette: the picture must change after each call.
static void mutateBorder(tic_mem* tic, s32 row, void* data)
{
    (void)tic;
    (void)data;

    assert(tic == Tic);
    assert(data == &callbackMarker);

    vbank0()->vars.border = (u8)(row % TIC_PALETTE_SIZE);
}

static void mutateScanline(tic_mem* tic, s32 row, void* data)
{
    (void)tic;
    (void)data;

    assert(tic == Tic);
    assert(data == &callbackMarker);

    tic_rgb* color = &vbank1()->palette.colors[row % TIC_PALETTE_SIZE];
    color->r = (u8)row;
    color->g = (u8)(row * 3);
    color->b = (u8)(255 - row);
}

static void reset(void)
{
    core()->state.vbank.id = 0;

    tic_vram* v0 = vbank0();
    tic_vram* v1 = vbank1();

    memset(v0->screen.data, 0, sizeof(tic_screen));
    memset(v1->screen.data, 0, sizeof(tic_screen));

    for(s32 i = 0; i < TIC_PALETTE_SIZE; i++)
    {
        v0->palette.colors[i] = (tic_rgb){(u8)(i * 17 + 3), (u8)(i * 5 + 7), (u8)(i * 11 + 1)};
        v1->palette.colors[i] = (tic_rgb){(u8)(255 - i * 13), (u8)(i * 29 + 2), (u8)(200 - i * 7)};
    }

    v0->vars.border = 3;
    v0->vars.clear = 0;
    v0->vars.offset.x = 0;
    v0->vars.offset.y = 0;

    v1->vars.border = 9;
    v1->vars.clear = 2;
    v1->vars.offset.x = 0;
    v1->vars.offset.y = 0;

    memset(Tic->product.screen, 0xff, sizeof(actual));
}

static void fill(tic_vram* vram, s32 seed)
{
    for(s32 i = 0; i < TIC80_WIDTH * TIC80_HEIGHT; i++)
        tic_tool_poke4(vram->screen.data, (u32)i, (u8)((i * 7 + seed * 5 + i / TIC80_WIDTH * 3) % TIC_PALETTE_SIZE));
}

static void fillSolid(tic_vram* vram, u8 color)
{
    for(s32 i = 0; i < TIC80_WIDTH * TIC80_HEIGHT; i++)
        tic_tool_poke4(vram->screen.data, (u32)i, color);
}

static void runCaseCall(const char* what, tic_blit_callback clb, bool defaultBlit)
{
    savedId = core()->state.vbank.id;
    savedMem = core()->state.vbank.mem;
    savedRam = core()->memory.ram->vram;

    borderCount = 0;
    scanCount = 0;

    if(defaultBlit)
        tic_core_blit(Tic);
    else
        tic_core_blit_ex(Tic, clb);

    memcpy(actual, Tic->product.screen, sizeof(actual));

    const s32 gotBorder = borderCount;
    const s32 gotScan = scanCount;
    s32 gotBorderLog[MAX_BORDER_ROWS], gotScanLog[MAX_SCAN_ROWS];
    memcpy(gotBorderLog, borderLog, gotBorder * sizeof(s32));
    memcpy(gotScanLog, scanLog, gotScan * sizeof(s32));

    // The reference starts from the same vram the implementation did.
    core()->state.vbank.id = savedId;
    core()->state.vbank.mem = savedMem;
    core()->memory.ram->vram = savedRam;

    borderCount = 0;
    scanCount = 0;
    refBlit(clb);

    assert(gotBorder == borderCount);
    assert(gotScan == scanCount);

    for(s32 i = 0; i < gotBorder; i++)
        assert(gotBorderLog[i] == borderLog[i]);

    for(s32 i = 0; i < gotScan; i++)
        assert(gotScanLog[i] == scanLog[i]);

    for(s32 i = 0; i < FULL_PIXELS; i++)
    {
        if(actual[i] != reference[i])
        {
            fprintf(stderr, "%s: (%d,%d) got %08x want %08x\n", what, i % TIC80_FULLWIDTH, i / TIC80_FULLWIDTH,
                    actual[i], reference[i]);
            assert(false);
        }
    }

    puts(what);
}

static void runCase(const char* what, tic_blit_callback clb)
{
    runCaseCall(what, clb, false);
}

static void testMixed(void)
{
    reset();
    fill(vbank0(), 1);
    fill(vbank1(), 6);

    runCase("blit mixed frame", (tic_blit_callback){0});
}

static void testClearHigh(void)
{
    reset();
    fill(vbank0(), 12);
    fill(vbank1(), 6);
    vbank1()->vars.clear = TIC_PALETTE_SIZE - 1;

    runCase("blit high clear frame", (tic_blit_callback){0});
}

static void testVbank1Clear(void)
{
    reset();
    fill(vbank0(), 2);
    fillSolid(vbank1(), vbank1()->vars.clear);

    runCase("blit vbank1 clear frame", (tic_blit_callback){0});
}

static void testVbank1Opaque(void)
{
    reset();
    fill(vbank0(), 4);

    // Every pixel is visible, so the vbank0 screen is never read.
    vbank1()->vars.clear = 0;
    for(s32 i = 0; i < TIC80_WIDTH * TIC80_HEIGHT; i++)
        tic_tool_poke4(vbank1()->screen.data, (u32)i, (u8)((i * 11 + 5) % (TIC_PALETTE_SIZE - 1) + 1));

    runCase("blit vbank1 opaque frame", (tic_blit_callback){0});
}

static void testOffsets(void)
{
    reset();
    fill(vbank0(), 7);
    fill(vbank1(), 9);
    vbank0()->vars.offset.x = 3;
    vbank0()->vars.offset.y = -7;
    vbank1()->vars.offset.x = -5;
    vbank1()->vars.offset.y = 11;
    runCase("blit offsets frame", (tic_blit_callback){0});

    reset();
    fill(vbank0(), 8);
    fill(vbank1(), 10);
    vbank0()->vars.offset.x = 127;
    vbank0()->vars.offset.y = -128;
    vbank1()->vars.offset.x = -128;
    vbank1()->vars.offset.y = 127;
    runCase("blit extreme offsets frame", (tic_blit_callback){0});

    // The row-end wrap of a one-pixel shift.
    reset();
    fill(vbank0(), 14);
    fill(vbank1(), 1);
    vbank0()->vars.offset.x = -1;
    vbank1()->vars.offset.x = 1;
    runCase("blit edge offsets frame", (tic_blit_callback){0});

    // Only one bank scrolls; the other one must keep its row start.
    reset();
    fill(vbank0(), 11);
    fill(vbank1(), 13);
    vbank1()->vars.offset.x = 7;
    vbank1()->vars.offset.y = -3;
    runCase("blit single bank offset frame", (tic_blit_callback){0});
}

static void testVbankSwap(void)
{
    reset();
    core()->state.vbank.id = 1;
    fill(vbank0(), 12);
    fill(vbank1(), 14);
    vbank0()->vars.border = 5;
    vbank1()->vars.clear = 3;

    runCase("blit vbank swapped frame", (tic_blit_callback){0});
}

static void testCallbacks(void)
{
    reset();
    fill(vbank0(), 3);
    fill(vbank1(), 5);

    tic_blit_callback clb = {
        .scanline = recordScanline,
        .border = recordBorder,
        .data = &callbackMarker,
    };

    runCase("blit callbacks frame", clb);

    assert(borderCount == MAX_BORDER_ROWS);
    assert(scanCount == MAX_SCAN_ROWS);

    for(s32 i = 0; i < borderCount; i++)
        assert(borderLog[i] == i);

    for(s32 i = 0; i < scanCount; i++)
        assert(scanLog[i] == i);
}

static void mutateBank0(tic_mem* tic, s32 row, void* data)
{
    (void)tic;
    (void)data;

    tic_rgb* color = &vbank0()->palette.colors[row % TIC_PALETTE_SIZE];
    color->r = (u8)(255 - row);
    color->g = (u8)(row * 7);
    color->b = (u8)(row * 11);
}

// Only the bottom border rows remain; the refresh must still happen there.
static void lateBorder(tic_mem* tic, s32 row, void* data)
{
    (void)tic;
    (void)data;

    if(row == 141)
        vbank0()->palette.colors[0] = (tic_rgb){1, 2, 3};
}

static void swapBanks(tic_mem* tic, s32 row, void* data)
{
    (void)data;

    if(row == 60)
        tic_api_vbank(tic, 1);
}

static void lateScanline(tic_mem* tic, s32 row, void* data)
{
    (void)tic;
    (void)data;

    // After several rows already composed through the tables.
    if(row == 40)
        vbank1()->palette.colors[5] = (tic_rgb){9, 8, 7};
}

static void quietBorder(tic_mem* tic, s32 row, void* data)
{
    (void)tic;
    (void)row;
    (void)data;
}

static void testInitializedCallbacks(void)
{
    reset();
    fill(vbank0(), 10);
    fill(vbank1(), 12);

    // The core's own callbacks, reached through tic_core_blit's wrappers; the
    // palette changes mid-frame, when the tables are already in use.
    core()->state.initialized = true;
    core()->state.callback.scanline = lateScanline;
    core()->state.callback.border = quietBorder;

    runCaseCall("blit initialized callbacks frame", (tic_blit_callback){.scanline = lateScanline, .border = quietBorder},
                true);

    core()->state.callback = (tic_blit_callback){0};
    core()->state.initialized = false;
}

static void testCallbackPalette(void)
{
    reset();
    fill(vbank0(), 15);
    fill(vbank1(), 2);

    tic_blit_callback clb = {
        .scanline = mutateScanline,
        .border = mutateBorder,
        .data = &callbackMarker,
    };

    runCase("blit callback palette frame", clb);
}

static void testDefaultBlit(void)
{
    reset();
    fill(vbank0(), 6);
    fill(vbank1(), 8);

    // The static callbacks forward nothing in an uninitialized core, so the
    // reference picture is the one without callbacks.
    runCaseCall("blit default frame", (tic_blit_callback){0}, true);
}

static void testBank0Palette(void)
{
    reset();
    fill(vbank0(), 11);
    fill(vbank1(), 4);

    runCase("blit vbank0 palette frame", (tic_blit_callback){.scanline = mutateBank0});
}

static void testBorderPalette(void)
{
    reset();
    fill(vbank0(), 0);
    fill(vbank1(), 3);
    vbank0()->vars.border = 0;

    runCase("blit border palette frame", (tic_blit_callback){.border = lateBorder});
}

static void testOffsetPalette(void)
{
    reset();
    fill(vbank0(), 5);
    fill(vbank1(), 7);
    vbank0()->vars.offset.x = 3;
    vbank0()->vars.offset.y = -7;
    vbank1()->vars.offset.x = -5;
    vbank1()->vars.offset.y = 11;

    runCase("blit offset palette frame",
            (tic_blit_callback){.scanline = mutateScanline, .border = mutateBorder, .data = &callbackMarker});
}

static void testBankSwap(void)
{
    reset();
    fill(vbank0(), 4);
    fill(vbank1(), 9);

    runCase("blit mid-frame bank swap", (tic_blit_callback){.scanline = swapBanks});
}

static void testFormat(tic80_pixel_color_format format, const char* name)
{
    printf("blit %s\n", name);

    Tic = tic_core_create(TIC80_SAMPLERATE, format);
    assert(Tic);

    testMixed();
    testClearHigh();
    testVbank1Clear();
    testVbank1Opaque();
    testOffsets();
    testVbankSwap();
    testDefaultBlit();
    testInitializedCallbacks();
    testCallbacks();
    testCallbackPalette();
    testBank0Palette();
    testBorderPalette();
    testOffsetPalette();
    testBankSwap();

    tic_core_close(Tic);
}

int main(void)
{
    setvbuf(stdout, NULL, _IONBF, 0);

    testFormat(TIC80_PIXEL_COLOR_RGBA8888, "rgba8888");
    testFormat(TIC80_PIXEL_COLOR_BGRA8888, "bgra8888");
    testFormat(TIC80_PIXEL_COLOR_ARGB8888, "argb8888");
    testFormat(TIC80_PIXEL_COLOR_ABGR8888, "abgr8888");

    puts("blit ok");

    return 0;
}
