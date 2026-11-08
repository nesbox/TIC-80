/* The Lua binding's error line: a message places an error only on the cart's
 * own chunk, so eval snippets and files never point into the cart's code.
 * Configure with -DBUILD_WITH_LUA=ON -DBUILD_TESTS=ON; ctest -R lua-test runs it.
 */
#include "core/core.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

#if defined(_WIN32) && !defined(TIC_RUNTIME_STATIC)
__declspec(dllimport)
#endif
extern const tic_script EXPORT_SCRIPT(Lua);

static const tic_script* Script = &EXPORT_SCRIPT(Lua);

static void setCode(tic_mem* tic, const char* code)
{
    snprintf(tic->cart.code.data, sizeof(tic->cart.code.data), "%s", code);
}

int main(void)
{
    tic_mem* tic = tic_core_create(TIC80_SAMPLERATE, TIC80_PIXEL_COLOR_RGBA8888);
    assert(tic);
    assert(Script->errorLine);

    setCode(tic, "-- title:  game\nfunction TIC()\nend\n");

    // The message's own position.
    assert(Script->errorLine(tic, "[string \"-- title:  game...\"]:8: attempt to index a nil value") == 8);

    // A message without a position falls back to the first cart frame.
    assert(Script->errorLine(tic,
                             "boom\n"
                             "stack traceback:\n"
                             "\t[C]: in ?\n"
                             "\t[string \"-- title:  game...\"]:5: in function 'TIC'") == 5);

    // An eval chunk and a file chunk are not the cart.
    assert(Script->errorLine(tic, "[string \"foo()\"]:1: boom") == 0);
    assert(Script->errorLine(tic, "@err.lua:3: boom") == 0);

    // A position in the message names the error's chunk: another chunk's
    // position is not replaced by a cart frame from the traceback.
    assert(Script->errorLine(tic,
                             "[string \"snippet...\"]:1: boom\n"
                             "stack traceback:\n"
                             "\t[string \"-- title:  game...\"]:8: in function 'TIC'") == 0);

    // A chunk id Lua did not cut can only be a one-line source, not a line of
    // a multi-line cart.
    setCode(tic, "function TIC()\nend\n");
    assert(Script->errorLine(tic, "[string \"function TIC(\"]:1: boom") == 0);
    assert(Script->errorLine(tic, "[string \"function TIC()\"]:1: boom") == 0);

    // A one-line cart's chunk id is the whole source and has no "...".
    setCode(tic, "function TIC() end");
    assert(Script->errorLine(tic, "[string \"function TIC() end\"]:1: boom") == 1);

    // Text without a chunk id places nothing.
    setCode(tic, "-- title:  game\nfunction TIC()\nend\n");
    assert(Script->errorLine(tic, "the code is empty") == 0);

    // An empty first line makes the chunk id ambiguous; it is rejected.
    setCode(tic, "\nfunction TIC() end\n");
    assert(Script->errorLine(tic, "[string \"...\"]:3: boom") == 0);

    // luaO_chunkid cuts a long first line at 45 bytes and appends "...".
    setCode(tic, "-- title:  a very long cart title that goes on and on and on forever\nfunction TIC() end\n");
    assert(Script->errorLine(tic, "[string \"-- title:  a very long cart title that goes o...\"]:5: boom") == 5);

    tic_core_close(tic);
    puts("Lua runtime tests passed");
    return 0;
}
