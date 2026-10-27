/* Run the Lugforth binding against the core without a window or audio device.
 * Configure with -DBUILD_WITH_LUGFORTH=ON -DBUILD_TESTS=ON; ctest -R lugforth-test runs it.
 * Besides the callbacks, it checks what the binding is for: a cartridge that
 * reaches for memory outside the module stops with an error, not the player.
 */
#include "core/core.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

#if defined(_WIN32) && !defined(TIC_RUNTIME_STATIC)
__declspec(dllimport)
#endif
extern const tic_script EXPORT_SCRIPT(Forth);

static const tic_script* Script = &EXPORT_SCRIPT(Forth);
static char Errors[4096];
static s32 ErrorCount;
static char Traces[4096];

static void onError(void* data, const char* message)
{
    (void)data;
    ErrorCount++;
    strncat(Errors, message, sizeof Errors - strlen(Errors) - 1);
}

static void onTrace(void* data, const char* text, u8 color)
{
    (void)data;
    (void)color;
    strncat(Traces, text, sizeof Traces - strlen(Traces) - 1);
    strncat(Traces, "|", sizeof Traces - strlen(Traces) - 1);
}

static void clearErrors(void)
{
    ErrorCount = 0;
    Errors[0] = '\0';
    Traces[0] = '\0';
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
    tic_tick_data data = {.error = onError, .trace = onTrace};
    core->data = &data;
    core->currentScript = Script;

    assert(strcmp(Script->name, "forth") == 0);

    assert(Script->init(tic,
                        "variable frames\n"
                        ": BOOT ( -- ) 1 2 pmem! ;\n"
                        ": TIC ( -- )\n"
                        "  0 cls  1 0 0 -1 1 0 0 1 1 spr\n"
                        "  12 5 3 pix!\n"
                        "  1 frames +!  frames @ 3 pmem! ;\n"
                        ": SCN ( row -- ) 4 pmem! ;\n"
                        ": BDR ( row -- ) 6 pmem! ;\n"
                        ": MENU ( index -- ) 7 pmem! ;\n"
                        "\\ a comment to the end of the line\n"
                        ".( hello) cr\n"));
    assert(ErrorCount == 0);
    assert(strstr(Traces, "hello|"));

    fillTile(&tic->ram->tiles.data[1], 2);
    Script->boot(tic);
    Script->tick(tic);
    assert(ErrorCount == 0);
    assert(tic->ram->persistent.data[2] == 1);
    assert(tic->ram->persistent.data[3] == 1);
    assert(pixel(tic, 12, 5) == 3);
    assert(pixel(tic, 0, 0) == 2);

    Script->callback.scanline(tic, 42, NULL);
    Script->callback.border(tic, 43, NULL);
    Script->callback.menu(tic, 3, NULL);
    assert(ErrorCount == 0);
    assert(tic->ram->persistent.data[4] == 42);
    assert(tic->ram->persistent.data[6] == 43);
    assert(tic->ram->persistent.data[7] == 3);

    // The console evaluates in the same dictionary.
    Script->eval(tic, "frames @ 100 + 8 pmem!  s\" ok\" 15 trace");
    assert(ErrorCount == 0);
    assert(tic->ram->persistent.data[8] == 101);
    assert(strstr(Traces, "ok|"));
    Script->tick(tic);
    assert(tic->ram->persistent.data[3] == 2);

    // An unknown word stops the cartridge with its name.
    clearErrors();
    assert(!Script->init(tic, ": TIC nosuchword ;"));
    assert(ErrorCount == 1);
    assert(strstr(Errors, "nosuchword"));
    assert(core->currentVM == NULL);

    // A word that leaves cells on the stack is reported, as with pForth.
    clearErrors();
    assert(Script->init(tic, ": TIC 1 2 ;"));
    Script->tick(tic);
    assert(ErrorCount == 1);
    assert(strstr(Errors, "stack overflow in TIC (2 items extra left)"));

    // THROW and division by zero are Forth errors, caught by the binding.
    clearErrors();
    assert(Script->init(tic, ": TIC 1 0 / drop ;"));
    Script->tick(tic);
    assert(ErrorCount == 1);
    assert(strstr(Errors, "THROW -10"));

    // The sandbox: an address past the module's memory traps in wasm3 and
    // stops the cartridge; nothing runs after it, and the player goes on.
    clearErrors();
    assert(Script->init(tic, ": TIC -4 @ drop  1 9 pmem! ;"));
    tic->ram->persistent.data[9] = 0;
    Script->tick(tic);
    assert(ErrorCount == 1);
    assert(strstr(Errors, "Out-of-bounds access in linear memory"));
    Script->tick(tic);
    assert(ErrorCount == 1);
    assert(tic->ram->persistent.data[9] == 0);

    clearErrors();
    assert(Script->init(tic, ": TIC 1 2000000000 ! ;"));
    Script->tick(tic);
    assert(ErrorCount == 1);

    // A forged execution token or stack pointer cannot leave the module either.
    clearErrors();
    assert(Script->init(tic, ": TIC 123456 execute -100 sp! 1 2 3 ;"));
    Script->tick(tic);
    assert(ErrorCount == 1);

    // Replacing a cartridge starts from a fresh dictionary.
    clearErrors();
    assert(Script->init(tic, ": TIC [defined] frames 10 pmem! ;"));
    Script->tick(tic);
    assert(ErrorCount == 0);
    assert(tic->ram->persistent.data[10] == 0);

    tic_core_close(tic);
    puts("Lugforth runtime tests passed");
    return 0;
}
