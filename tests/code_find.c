/* The search model links without the studio: this file builds code_find.c and
 * code_text.c with the history module alone, and a test script whose outline
 * is a fixed list.
 *
 * Target code-find-test in cmake/tests.cmake; `ctest -R code-find` runs it.
 */
#include "studio/editors/code/find.h"
#include "script.h"

#include <assert.h>
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static char Buffer[TIC_CODE_SIZE];
static Text Text_;

static bool isAlnum(char c)
{
    return isalnum((u8)c) || c == '_';
}

static tic_outline_item Outline[8];
static s32 OutlineSize;

static const tic_outline_item* getOutline(const char* code, s32* size)
{
    (void)code;
    *size = OutlineSize;
    return Outline;
}

static const tic_script Script =
    {
        .name = "test",
        .singleComment = "--",
        .stdStringStartEnd = "\"",
        .getOutline = getOutline,
        .lang_isalnum = isAlnum,
};

static const tic_script SlashScript =
    {
        .name = "slash",
        .singleComment = "//",
        .stdStringStartEnd = "\"",
        .getOutline = getOutline,
        .lang_isalnum = isAlnum,
};

static void setup(const char* initial)
{
    text_free(&Text_);
    memset(Buffer, 0, sizeof Buffer);
    strncpy(Buffer, initial, MAX_CODE);

    text_init(&Text_, Buffer, (TextOptions){.script = &Script, .tabSize = 4}, NULL, NULL);
}

static void item(s32 index, char* pos, s32 size)
{
    Outline[index] = (tic_outline_item){.pos = pos, .size = size};
}

static void testFilter(void)
{
    assert(isFilterMatch("function", 8, "fn"));
    assert(isFilterMatch("function", 8, "FN"));
    assert(isFilterMatch("function", 8, ""));
    assert(!isFilterMatch("function", 8, "fx"));
    assert(!isFilterMatch("abc", 3, "abcd"));

    puts("filter ok");
}

static void testOutline(void)
{
    setup("aaa\nbbb\nccc\n");

    // Out of order, one inside a comment, one filtered out.
    item(0, Buffer + 8, 3);
    item(1, Buffer, 3);
    item(2, Buffer + 4, 3);
    OutlineSize = 3;
    getState(&Text_, Buffer + 4)->syntax = SyntaxType_COMMENT;

    CodeItems items;
    code_outline(&Text_, "", &items);

    assert(items.size == 2);
    assert(items.items[0].pos == Buffer);
    assert(items.items[1].pos == Buffer + 8);
    free(items.items);

    // The filter keeps the subsequence, ignoring case.
    code_outline(&Text_, "CC", &items);
    assert(items.size == 1);
    assert(items.items[0].pos == Buffer + 8);
    free(items.items);

    puts("outline ok");
}

static void testBookmarks(void)
{
    setup("-- @@@ first\ncode\n-- @@@ second\n");
    parseSyntaxColor(&Text_);

    CodeItems items;
    code_bookmarks(&Text_, &items);

    assert(items.size == 2);
    assert(items.items[0].pos == Buffer + 7);
    assert(items.items[0].size == 5);
    assert(items.items[1].pos == Buffer + 25);
    assert(items.items[1].size == 6);
    free(items.items);

    // The nearest bookmark line in each direction, at its note.
    assert(code_bookmark_next(&Text_, Buffer) == Buffer + 7);
    assert(code_bookmark_next(&Text_, Buffer + 1) == Buffer + 25);
    assert(code_bookmark_next(&Text_, Buffer + 8) == Buffer + 25);
    assert(code_bookmark_next(&Text_, Buffer + 25) == NULL);
    assert(code_bookmark_next(&Text_, Buffer + 32) == NULL);

    assert(code_bookmark_prev(&Text_, Buffer + 30) == Buffer + 25);
    assert(code_bookmark_prev(&Text_, Buffer + 8) == Buffer + 7);
    assert(code_bookmark_prev(&Text_, Buffer) == Buffer + 7);
    assert(code_bookmark_prev(&Text_, Buffer + 32) == Buffer + 25);

    puts("bookmarks ok");
}

static void testBookmarkForms(void)
{
    CodeItems items;

    // An empty note is an item at the line end.
    setup("-- @@@\n");
    parseSyntaxColor(&Text_);
    code_bookmarks(&Text_, &items);

    assert(items.size == 1);
    assert(items.items[0].pos == Buffer + 6);
    assert(items.items[0].size == 0);
    free(items.items);

    // An indented line, a tag that is not the comment's first token, a longer
    // run and a tag in a string are not bookmarks.
    setup("  -- @@@ note\n-- print(\"@@@\")\n-- @@@@\nprint(\"@@@\")\n");
    parseSyntaxColor(&Text_);
    code_bookmarks(&Text_, &items);

    assert(items.size == 0);
    assert(items.items == NULL);

    // The language's own single-line comment.
    setup("// @@@ note\n");
    Text_.options.script = &SlashScript;
    parseSyntaxColor(&Text_);
    code_bookmarks(&Text_, &items);

    assert(items.size == 1);
    assert(items.items[0].pos == Buffer + 7);
    assert(items.items[0].size == 4);
    free(items.items);

    puts("bookmark forms ok");
}

static void testGoto(void)
{
    setup("aa\nbb\ncc");

    s32 line = -1;
    const char* pos = code_goto_line(&Text_, "2", &line);
    assert(line == 1);
    assert(pos == Buffer + 3);

    pos = code_goto_line(&Text_, "", &line);
    assert(line == 0);
    assert(pos == Buffer);

    // Past the end, the last line; a number within it is 1-based.
    pos = code_goto_line(&Text_, "99", &line);
    assert(line == 2);
    assert(pos == Buffer + 6);

    // Before the first line the caret lands at the end, as the editor did.
    pos = code_goto_line(&Text_, "-1", &line);
    assert(line < 0);
    assert(pos == Buffer + strlen(Buffer));

    puts("goto ok");
}

static void testSearch(void)
{
    setup("foo bar foo");

    assert(code_search_down(Buffer, Buffer, "bar") == Buffer + 4);
    assert(code_search_down(Buffer, Buffer + 4, "foo") == Buffer + 8);
    assert(code_search_down(Buffer, Buffer, "baz") == NULL);

    assert(code_search_up(Buffer, Buffer + 11, "foo") == Buffer + 8);
    assert(code_search_up(Buffer, Buffer + 4, "foo") == Buffer);
    assert(code_search_up(Buffer, Buffer + 11, "baz") == NULL);

    puts("search ok");
}

int main(void)
{
    testFilter();
    testOutline();
    testBookmarks();
    testBookmarkForms();
    testGoto();
    testSearch();

    return 0;
}
