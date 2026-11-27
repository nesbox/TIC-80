/* The UTF-8 to Latin-1 mapping the JS and Python bindings do before the core
 * sees their strings: the byte mapping alone, no runtime. */
#include "tools.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void expectMap(const char* text, const char* expected, s32 size)
{
    char* copy = tic_tool_utf8_latin1(text);
    assert(copy);
    assert(memcmp(copy, expected, size) == 0);
    assert(copy[size] == 0);
    free(copy);
}

static void testLatin1(void)
{
    // Pure ASCII needs no copy.
    assert(tic_tool_utf8_latin1("print A") == NULL);
    assert(tic_tool_utf8_latin1("") == NULL);
    assert(tic_tool_utf8_latin1(NULL) == NULL);

    // U+0080, U+00B3 and U+00FF become one byte each.
    expectMap("\xC2\x80", "\x80", 1);
    expectMap("\xC2\xB3", "\xB3", 1);
    expectMap("\xC3\xBF", "\xFF", 1);

    // A Latin-1 code point between ASCII bytes shrinks the string.
    static const char Mixed[] = {'A', (char)0xC2, (char)0xB3, 'B', 0};
    static const char Mapped[] = {'A', (char)0xB3, 'B', 0};
    char* copy = tic_tool_utf8_latin1(Mixed);
    assert(copy && strcmp(copy, Mapped) == 0);
    free(copy);

    // Other code points and invalid sequences are copied as they are.
    assert(tic_tool_utf8_latin1("\xCE\xA9") == NULL);
    assert(tic_tool_utf8_latin1("\xE2\x82\xAC") == NULL);

    static const char Lone[] = {'A', (char)0xC2, 'B', 0};
    assert(tic_tool_utf8_latin1(Lone) == NULL);

    // The pair maps, the Greek code point and the lone lead byte stay.
    static const char Both[] = {(char)0xC2, (char)0xB3, (char)0xCE, (char)0xA9, 'A', (char)0xC3, (char)0xB1, 0};
    static const char BothMapped[] = {(char)0xB3, (char)0xCE, (char)0xA9, 'A', (char)0xF1, 0};
    copy = tic_tool_utf8_latin1(Both);
    assert(copy && memcmp(copy, BothMapped, sizeof BothMapped) == 0);
    free(copy);

    static const char Tail[] = {(char)0xC2, 'A', (char)0xC3, (char)0xB1, 0};
    static const char TailMapped[] = {(char)0xC2, 'A', (char)0xF1, 0};
    copy = tic_tool_utf8_latin1(Tail);
    assert(copy && memcmp(copy, TailMapped, sizeof TailMapped) == 0);
    free(copy);

    puts("latin1 ok");
}

int main(void)
{
    testLatin1();
    puts("tools ok");
    return 0;
}
