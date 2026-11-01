/* The project cart's text format without the studio: a cart written into a
 * buffer and read back, the comment coming from the script registry by the
 * file's name. The malformed texts are hand-written, since a project file is
 * a text file and an author can edit it.
 *
 * Target project-test in cmake/tests.cmake; `ctest -R project-test` runs it.
 */
#include "studio/project.h"
#include "tools.h"

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// Lua is the language every test build links; its comment is "--".
#define PROJECT "cart.lua"

// The caller's buffer holds a few times the cart: rom.c saves into
// TIC_CART_SAVE_SIZE * 3, and this roomier one lets the test see the size.
enum
{
    SaveSize = TIC_CART_SAVE_SIZE * 4,
};

static const char Code[] = "-- title: project test\nfunction TIC()\nend";

static tic_cartridge* newCart(void)
{
    tic_cartridge* cart = malloc(sizeof(tic_cartridge));
    assert(cart);
    memset(cart, 0, sizeof(tic_cartridge));

    return cart;
}

// Every byte set, so every section and every row of it carries data.
static void fillBank(tic_bank* bank, u8 seed)
{
    for(s32 i = 0; i < (s32)sizeof(tic_bank); i++)
        ((u8*)bank)[i] = (u8)(seed + i * 7);
}

static char* saveCartInto(const char* name, const tic_cartridge* cart, s32* size)
{
    char* text = malloc(SaveSize);
    assert(text);

    *size = tic_project_save(name, text, cart);
    assert(*size > 0);
    assert(*size < SaveSize);

    return text;
}

static tic_cartridge* loadText(const char* name, const char* text, s32 size, bool* ok)
{
    tic_cartridge* cart = newCart();
    *ok = tic_project_load(name, text, size, cart);

    return cart;
}

// The same text with CRLF line ends, as an editor on another system leaves it.
static char* crlfCopy(const char* text, s32 size, s32* outSize)
{
    char* out = malloc(size * 2 + 1);
    assert(out);

    s32 n = 0;

    for(s32 i = 0; i < size; i++)
    {
        if(text[i] == '\n')
            out[n++] = '\r';

        out[n++] = text[i];
    }

    out[n] = '\0';
    *outSize = n;

    return out;
}

static void testRoundTrip(void)
{
    tic_cartridge* cart = newCart();
    memcpy(cart->code.data, Code, sizeof Code);
    fillBank(&cart->banks[0], 1);
    fillBank(&cart->banks[5], 2);
    cart->lang = 3;

    s32 size = 0;
    char* text = saveCartInto(PROJECT, cart, &size);

    // What rom.c gives a project: three times the cart's own size.
    assert(size < TIC_CART_SAVE_SIZE * 3);

    bool ok = false;
    tic_cartridge* got = loadText(PROJECT, text, size, &ok);

    assert(ok);
    assert(memcmp(cart, got, sizeof(tic_cartridge)) == 0);

    free(got);
    free(text);
    free(cart);
    puts("round trip ok");
}

// Code only: the writer's line end must not become part of the code, and a
// code that ends with one of its own keeps it.
static void checkCodeOnly(const char* code)
{
    tic_cartridge* cart = newCart();
    memcpy(cart->code.data, code, strlen(code) + 1);

    s32 size = 0;
    char* text = saveCartInto(PROJECT, cart, &size);
    assert(size == (s32)strlen(code) + 1);

    bool ok = false;
    tic_cartridge* got = loadText(PROJECT, text, size, &ok);

    assert(ok);
    assert(strcmp(got->code.data, code) == 0);

    free(got);
    free(text);
    free(cart);
}

static void testCodeOnly(void)
{
    checkCodeOnly(Code);
    checkCodeOnly("print(1)\nprint(2)\n");

    puts("code only ok");
}

static void testEmptySections(void)
{
    static const char* const Below[] = {"SPRITES", "MAP", "WAVES", "SFX",
                                        "PATTERNS", "TRACKS", "FLAGS", "SCREEN", "PALETTE", "LANG"};
    tic_cartridge* cart = newCart();
    memcpy(cart->code.data, Code, sizeof Code);
    tic_tool_poke4(cart->banks[0].tiles.data[0].data, 0, 0xa);

    s32 size = 0;
    char* text = saveCartInto(PROJECT, cart, &size);

    assert(strstr(text, "<TILES>"));
    assert(!strstr(text, "<TILES1>"));

    for(s32 i = 0; i < COUNT_OF(Below); i++)
        assert(!strstr(text, Below[i]));

    bool ok = false;
    tic_cartridge* got = loadText(PROJECT, text, size, &ok);

    assert(ok);
    assert(memcmp(cart, got, sizeof(tic_cartridge)) == 0);

    free(got);
    free(text);
    free(cart);
    puts("empty sections ok");
}

// A cart with no code still has its data: the file opens with a section, and
// that is what an empty code section reads as.
static void testEmptyCode(void)
{
    tic_cartridge* cart = newCart();
    tic_tool_poke4(cart->banks[0].tiles.data[0].data, 0, 0xa);
    fillBank(&cart->banks[2], 3);

    s32 size = 0;
    char* text = saveCartInto(PROJECT, cart, &size);
    assert(strncmp(text, "-- <", 4) == 0);

    bool ok = false;
    tic_cartridge* got = loadText(PROJECT, text, size, &ok);

    assert(ok);
    assert(got->code.data[0] == '\0');
    assert(memcmp(cart, got, sizeof(tic_cartridge)) == 0);

    free(got);
    free(text);
    free(cart);
    puts("empty code ok");
}

// Nothing to load: an empty buffer is refused, and the cart it was given
// stays as it was.
static void testEmptyFile(void)
{
    tic_cartridge* target = newCart();
    memset(target, 0x5a, sizeof(tic_cartridge));
    tic_cartridge* before = malloc(sizeof(tic_cartridge));
    assert(before);
    memcpy(before, target, sizeof(tic_cartridge));

    static const char Nothing[] = "";

    bool ok = tic_project_load(PROJECT, Nothing, 0, target);
    assert(!ok);
    assert(memcmp(target, before, sizeof(tic_cartridge)) == 0);

    free(before);
    free(target);
    puts("empty file ok");
}

// Nothing to save either: no code and no data is no file.
static void testNothingToSave(void)
{
    tic_cartridge* cart = newCart();

    char* text = malloc(SaveSize);
    assert(text);
    assert(tic_project_save(PROJECT, text, cart) == 0);

    free(text);
    free(cart);
    puts("nothing to save ok");
}

static void testLineEndings(void)
{
    tic_cartridge* cart = newCart();
    memcpy(cart->code.data, Code, sizeof Code);
    fillBank(&cart->banks[0], 1);

    s32 size = 0;
    char* text = saveCartInto(PROJECT, cart, &size);

    s32 crlfSize = 0;
    char* crlf = crlfCopy(text, size, &crlfSize);

    bool ok = false;
    tic_cartridge* got = loadText(PROJECT, crlf, crlfSize, &ok);

    assert(ok);
    assert(memcmp(cart, got, sizeof(tic_cartridge)) == 0);

    free(got);
    free(crlf);
    free(text);
    free(cart);
    puts("line endings ok");
}

// A row shorter than a row is no row: it is not read, and the section ends.
static void testShortRow(void)
{
    static const char Text[] = "print(1)\n-- <TILES>\n-- 000: 12\n-- </TILES>\n\n";

    bool ok = false;
    tic_cartridge* got = loadText(PROJECT, Text, sizeof Text - 1, &ok);

    assert(ok);
    assert(strcmp(got->code.data, "print(1)") == 0);

    for(s32 i = 0; i < (s32)sizeof(tic_tiles); i++)
        assert(((u8*)&got->banks[0].tiles)[i] == 0);

    free(got);
    puts("short row ok");
}

// A negative row index is not a row either: the old parser wrote before the
// section, into the screen that sits ahead of the tiles in a bank.
static void testBadRowIndex(void)
{
    char hex[sizeof(tic_tile) * 2 + 1];
    memset(hex, '1', sizeof(hex) - 1);
    hex[sizeof(hex) - 1] = '\0';

    char text[256];
    s32 size = snprintf(text, sizeof text, "print(1)\n-- <TILES>\n-- -01: %s\n-- </TILES>\n\n", hex);
    assert(size > 0 && size < (s32)sizeof text);

    bool ok = false;
    tic_cartridge* got = loadText(PROJECT, text, size, &ok);

    assert(ok);
    assert(strcmp(got->code.data, "print(1)") == 0);

    for(s32 i = 0; i < (s32)sizeof(tic_screen); i++)
        assert(got->banks[0].screen.data[i] == 0);

    for(s32 i = 0; i < (s32)sizeof(tic_tiles); i++)
        assert(((u8*)&got->banks[0].tiles)[i] == 0);

    free(got);
    puts("bad row index ok");
}

// The code is a buffer, not a string: a cart can fill it to the last byte.
static void testFullCode(void)
{
    tic_cartridge* cart = newCart();
    memset(cart->code.data, 'a', TIC_CODE_SIZE);
    memset(cart->binary.data, 'b', 8);

    s32 size = 0;
    char* text = saveCartInto(PROJECT, cart, &size);

    // The code and the writer's line end; nothing read past the buffer.
    assert(size == TIC_CODE_SIZE + 1);

    bool ok = false;
    tic_cartridge* got = loadText(PROJECT, text, size, &ok);

    assert(ok);
    assert(memcmp(got->code.data, cart->code.data, TIC_CODE_SIZE) == 0);

    free(got);
    free(text);
    free(cart);
    puts("full code ok");
}

// A name no script knows has no comment to write or read by.
static void testUnknownName(void)
{
    tic_cartridge* cart = newCart();
    memcpy(cart->code.data, Code, sizeof Code);

    char* text = malloc(SaveSize);
    assert(text);
    assert(tic_project_save("cart.bin", text, cart) == 0);

    tic_cartridge* target = newCart();
    memset(target, 0x5a, sizeof(tic_cartridge));
    tic_cartridge* before = malloc(sizeof(tic_cartridge));
    assert(before);
    memcpy(before, target, sizeof(tic_cartridge));

    static const char Text[] = "-- <TILES>\n\n";

    bool ok = tic_project_load("cart.bin", Text, sizeof Text - 1, target);
    assert(!ok);
    assert(memcmp(target, before, sizeof(tic_cartridge)) == 0);

    free(before);
    free(target);
    free(text);
    free(cart);
    puts("unknown name ok");
}

#if defined(PROJECT_TEST_PYTHON)
// A one-character comment: the row prefix loses a byte of the comment.
static void testOtherComment(void)
{
    static const char PyCode[] = "# title: project test\ndef TIC():\n    pass";

    tic_cartridge* cart = newCart();
    memcpy(cart->code.data, PyCode, sizeof PyCode);
    fillBank(&cart->banks[0], 1);
    fillBank(&cart->banks[3], 2);
    cart->lang = 1;

    s32 size = 0;
    char* text = saveCartInto("cart.py", cart, &size);
    assert(strstr(text, "# <TILES>"));

    bool ok = false;
    tic_cartridge* got = loadText("cart.py", text, size, &ok);

    assert(ok);
    assert(memcmp(cart, got, sizeof(tic_cartridge)) == 0);

    free(got);
    free(text);
    free(cart);
    puts("other comment ok");
}
#endif

int main(void)
{
    testRoundTrip();
    testCodeOnly();
    testEmptySections();
    testEmptyCode();
    testEmptyFile();
    testNothingToSave();
    testLineEndings();
    testShortRow();
    testBadRowIndex();
    testFullCode();
    testUnknownName();
#if defined(PROJECT_TEST_PYTHON)
    testOtherComment();
#endif

    puts("project ok");
    return 0;
}
