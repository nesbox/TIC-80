/* Run the JavaScript binding against the core without a window or audio device.
 * Configure with -DBUILD_WITH_JS=ON -DBUILD_TESTS=ON; ctest -R js-test runs it.
 * Linking the runtime target exercises both shared and static builds.
 */
#include "core/core.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

#if defined(_WIN32) && !defined(TIC_RUNTIME_STATIC)
__declspec(dllimport)
#endif
extern const tic_script EXPORT_SCRIPT(Js);

static const tic_script* Script = &EXPORT_SCRIPT(Js);
static char Errors[4096];
static s32 ErrorCount;

static void onError(void* data, const char* message)
{
    ErrorCount++;
    strncat(Errors, message, sizeof Errors - strlen(Errors) - 1);
}

static void clearErrors(void)
{
    ErrorCount = 0;
    Errors[0] = '\0';
}

static void fillTile(tic_tile* tile, u8 color)
{
    for(s32 i = 0; i < TIC_SPRITESIZE * TIC_SPRITESIZE; i++)
        tic_tool_poke4(tile->data, i, color);
}

static u8 pixel(tic_mem* tic, s32 x, s32 y)
{
    return tic_tool_peek4(tic->ram->vram.screen.data, y * TIC80_WIDTH + x);
}

int main(void)
{
    tic_mem* tic = tic_core_create(TIC80_SAMPLERATE, TIC80_PIXEL_COLOR_RGBA8888);
    assert(tic);
    tic_core* core = (tic_core*)tic;
    tic_tick_data data = {.error = onError};
    core->data = &data;
    core->currentScript = Script;

    // Evaluating before a cartridge has created a VM must be harmless.
    Script->eval(tic, "throw new Error('no VM')");
    assert(ErrorCount == 0);

    assert(Script->init(tic,
        "let frames = 0;"
        "let ready = false;"
        "Promise.resolve(7).then(value => { pmem(1, value); ready = true; });"
        "function BOOT() { pmem(2, 1); }"
        "function TIC() {"
        "  if (!ready) throw new Error('pending job did not run');"
        "  cls();"
        "  pix(4, 5, 3);"
        "  spr(1, 0, 0, [0]);"
        "  map(0, 0, 1, 1, 16, 0, [0], 1, () => [2, 0, 0]);"
        "  sfx(-1, 0, -1, 0, [3, 5]);"
        "  pmem(3, ++frames);"
        "}"
        "function SCN(row) { pmem(4, row); }"
        "function scanline(row) { pmem(5, row); }"
        "function BDR(row) { pmem(6, row); }"
        "function MENU(index) { pmem(7, index); }"));

    fillTile(&tic->ram->tiles.data[1], 2);
    fillTile(&tic->ram->tiles.data[2], 4);
    Script->boot(tic);
    Script->tick(tic);
    assert(ErrorCount == 0);
    assert(tic->ram->persistent.data[1] == 7);
    assert(tic->ram->persistent.data[2] == 1);
    assert(tic->ram->persistent.data[3] == 1);
    assert(pixel(tic, 0, 0) == 2);
    assert(pixel(tic, 16, 0) == 4);

    // The old API also recognized nested proxies and accepted coercible values.
    Script->eval(tic,
        "const proxy = array => new Proxy(new Proxy(array, {}), {});"
        "spr(1, 32, 0, proxy([new Number(0)]));"
        "map(0, 0, 1, 1, 48, 0, proxy(['0']), 1,"
        "    () => proxy([new Number(2), '0', new Number(0)]));"
        "sfx(-1, 0, -1, 0, proxy([new Number(3), '5']));"
        "ttri(64, 0, 72, 0, 64, 8, 0, 0, 8, 0, 0, 8, 0, proxy([new Number(0)]));"
        "if (typeof textri === 'function')"
        "  textri(80, 0, 88, 0, 80, 8, 0, 0, 8, 0, 0, 8, false, proxy(['0']));");
    assert(ErrorCount == 0);
    assert(pixel(tic, 32, 0) == 2);
    assert(pixel(tic, 48, 0) == 4);

    Script->callback.scanline(tic, 42, NULL);
    Script->callback.border(tic, 43, NULL);
    Script->callback.menu(tic, 3, NULL);
    assert(ErrorCount == 0);
    assert(tic->ram->persistent.data[4] == 42);
    assert(tic->ram->persistent.data[5] == 42);
    assert(tic->ram->persistent.data[6] == 43);
    assert(tic->ram->persistent.data[7] == 3);

    Script->eval(tic, "pmem(8, Number(123n) + /tic/i.test('TIC'));");
    assert(ErrorCount == 0);
    assert(tic->ram->persistent.data[8] == 124);
    Script->tick(tic);
    assert(tic->ram->persistent.data[3] == 2);

    // Native Error objects still report both the message and the JS stack.
    Script->eval(tic, "throw new Error('eval failure')");
    assert(ErrorCount >= 2);
    assert(strstr(Errors, "eval failure"));
    assert(strstr(Errors, "<eval>"));
    clearErrors();

    assert(Script->init(tic, "function TIC() { throw new Error('tick failure'); }"));
    Script->tick(tic);
    assert(ErrorCount >= 2);
    assert(strstr(Errors, "tick failure"));
    assert(strstr(Errors, "index.js"));
    clearErrors();

    assert(!Script->init(tic, "function TIC( {"));
    assert(strstr(Errors, "SyntaxError"));
    assert(core->currentVM == NULL);
    clearErrors();

    // Replacing a cartridge closes the failed VM and starts with fresh globals.
    assert(Script->init(tic, "function TIC() { pmem(9, typeof frames === 'undefined'); }"));
    Script->tick(tic);
    assert(ErrorCount == 0);
    assert(tic->ram->persistent.data[9] == 1);

    tic_core_close(tic);
    puts("JavaScript runtime tests passed");
    return 0;
}
