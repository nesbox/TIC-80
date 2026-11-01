/* The code editor's text model links without the studio: this file builds
 * code_text.c with the history module alone, and no editor or host in sight.
 *
 * Target code-text-test (and code-text-test-pro for the PRO cap) in
 * cmake/tests.cmake; `ctest -R code-text` runs both.
 */
#include "studio/editors/code/text.h"
#include "script.h"

#include <assert.h>
#include <ctype.h>
#include <stdio.h>
#include <string.h>

static char Buffer[TIC_CODE_SIZE];
static Text Text_;
static TextOptions Options;

static bool isAlnum(char c)
{
    return isalnum((u8)c) || c == '_';
}

static const char* Keywords[] = {"if", "then", "end"};

static const tic_script Script =
    {
        .name = "test",
        .singleComment = "--",
        .blockCommentStart = "--[[",
        .blockCommentEnd = "]]",
        .stdStringStartEnd = "\"",
        .keywords = Keywords,
        .keywordsCount = COUNT_OF(Keywords),
        .lang_isalnum = isAlnum,
};

static void setup(const char* initial)
{
    text_free(&Text_);
    memset(Buffer, 0, sizeof Buffer);
    strncpy(Buffer, initial, MAX_CODE);

    Options.script = &Script;
    Options.tabSize = 4;

    text_init(&Text_, Buffer, Options, NULL, NULL);
}

static void testCap(void)
{
    setup("");

    memset(Buffer, 'a', MAX_CODE);
    Buffer[MAX_CODE] = '\0';
    Text_.cursor.position = Buffer + MAX_CODE;

    inputSymbol(&Text_, 'x');

    assert(strlen(Buffer) == (size_t)MAX_CODE);
    assert(Buffer[MAX_CODE] == '\0');
    assert(Buffer[MAX_CODE - 1] == 'a');

    // The last free byte is the limit, and taking it fills the buffer.
    memset(Buffer, 'a', MAX_CODE - 1);
    Buffer[MAX_CODE - 1] = '\0';
    Text_.cursor.position = Buffer + MAX_CODE - 1;

    inputSymbol(&Text_, 'x');

    assert(strlen(Buffer) == (size_t)MAX_CODE);
    assert(Buffer[MAX_CODE - 1] == 'x');
    assert(Buffer[MAX_CODE] == '\0');

    // The primitive refuses an insertion past the cap too.
    insertCode(&Text_, Buffer + MAX_CODE, "yz");

    assert(strlen(Buffer) == (size_t)MAX_CODE);
    assert(Buffer[MAX_CODE] == '\0');

    puts("cap ok");
}

static void testState(void)
{
    setup("abc");

    // The caret's byte is the one the snapshot marks.
    assert(getState(&Text_, Buffer)->cursor == 1);
    assert(getState(&Text_, Buffer + 1)->cursor == 0);

    Text_.cursor.position = Buffer + 1;
    insertCode(&Text_, Buffer + 1, "X");

    assert(strcmp(Buffer, "aXbc") == 0);

    // The inserted byte gets a fresh state; the bytes after it keep theirs.
    assert(getState(&Text_, Buffer + 1)->sym == '\0');
    assert(getState(&Text_, Buffer + 2)->sym == 'b');
    assert(getState(&Text_, Buffer + 3)->sym == 'c');

    // Deleting moves the state back with the bytes.
    deleteCode(&Text_, Buffer + 1, Buffer + 2);

    assert(strcmp(Buffer, "abc") == 0);
    assert(getState(&Text_, Buffer + 1)->sym == 'b');
    assert(getState(&Text_, Buffer + 2)->sym == 'c');

    puts("state ok");
}

static void testUndoRedo(void)
{
    setup("abc\ndef\n");

    Text_.cursor.position = Buffer + 8;
    inputSymbol(&Text_, 'X');

    assert(strcmp(Buffer, "abc\ndef\nX") == 0);

    char* edited = Buffer + 8;

    text_undo(&Text_);

    // The text is back, and the caret sits on the first byte the edit changed.
    assert(strcmp(Buffer, "abc\ndef\n") == 0);
    assert(Text_.cursor.position == edited);

    text_redo(&Text_);

    assert(strcmp(Buffer, "abc\ndef\nX") == 0);
    assert(Text_.cursor.position == edited + 1);

    puts("undo/redo ok");
}

static void testNavigation(void)
{
    setup("one two\nthree\n");

    assert(getLineByPos(&Text_, Buffer + 5) == Buffer);
    assert(getLineByPos(&Text_, Buffer + 10) == Buffer + 8);
    assert(getPrevLineByPos(&Text_, Buffer + 8) == Buffer);
    assert(getNextLineByPos(&Text_, Buffer) == Buffer + 8);
    assert(getLineSize(Buffer) == 7);
    assert(getLineSize(Buffer + 8) == 5);

    Text_.cursor.position = Buffer + 11;
    leftWord(&Text_);
    assert(Text_.cursor.position == Buffer + 8);

    Text_.cursor.position = Buffer + 8;
    rightWord(&Text_);
    assert(Text_.cursor.position == Buffer + 13);

    // Balanced expressions and quotes.
    setup("(a (b) c)");
    assert(leftSexp(&Text_, Buffer + 4) == Buffer + 3);
    assert(rightSexp(&Text_, Buffer + 4) == Buffer + 5);
    assert(findMatchedDelim(&Text_, Buffer + 3) == Buffer + 5);
    assert(findMatchedDelim(&Text_, Buffer + 5) == Buffer + 3);

    setup("say \"hi\" now");
    assert(leftQuote(&Text_, Buffer + 6) == Buffer + 4);
    assert(rightQuote(&Text_, Buffer + 5) == Buffer + 7);

    puts("navigation ok");
}

static const tic_script AltScript =
    {
        .name = "alt",
        .singleComment = ";",
        .stdStringStartEnd = "'",
        .lang_isalnum = isAlnum,
};

static const tic_script NoCommentScript =
    {
        .name = "none",
        .stdStringStartEnd = "\"",
        .lang_isalnum = isAlnum,
};

static void testSyntaxOtherLanguage(void)
{
    setup("-- note\n; note\n");
    Text_.options.script = &AltScript;
    parseSyntaxColor(&Text_);

    // In this language "--" is not a comment and ";" is.
    assert(getState(&Text_, Buffer + 1)->syntax != SyntaxType_COMMENT);
    assert(getState(&Text_, Buffer + 10)->syntax == SyntaxType_COMMENT);

    puts("syntax other ok");
}

static void testBookmarkLine(void)
{
    s32 size = 0;
    const char* note = NULL;

    setup("-- @@@ physics\n");
    parseSyntaxColor(&Text_);
    note = text_bookmark_note(&Text_, Buffer, &size);
    assert(note == Buffer + 7);
    assert(size == 7);

    // Spaces after the tag and trailing spaces around the note.
    setup("-- @@@   boss fight    \n");
    parseSyntaxColor(&Text_);
    note = text_bookmark_note(&Text_, Buffer, &size);
    assert(note == Buffer + 9);
    assert(size == 10);

    setup("-- @@@\n");
    parseSyntaxColor(&Text_);
    note = text_bookmark_note(&Text_, Buffer, &size);
    assert(note == Buffer + 6);
    assert(size == 0);

    // The language's own single-line comment.
    setup("; @@@ note\n");
    Text_.options.script = &AltScript;
    parseSyntaxColor(&Text_);
    note = text_bookmark_note(&Text_, Buffer, &size);
    assert(note == Buffer + 6);
    assert(size == 4);

    // An indented marker is an ordinary comment.
    setup("  -- @@@ physics\n");
    parseSyntaxColor(&Text_);
    assert(text_bookmark_note(&Text_, Buffer, NULL) == NULL);

    // The tag is not the comment's first token.
    setup("-- print(\"@@@\")\n");
    parseSyntaxColor(&Text_);
    assert(text_bookmark_note(&Text_, Buffer, NULL) == NULL);

    // A longer run is not the tag.
    setup("-- @@@@\n");
    parseSyntaxColor(&Text_);
    assert(text_bookmark_note(&Text_, Buffer, NULL) == NULL);

    // A tag outside a comment.
    setup("print(\"@@@\")\n");
    parseSyntaxColor(&Text_);
    assert(text_bookmark_note(&Text_, Buffer, NULL) == NULL);

    // No script, or a language without a single-line comment.
    setup("-- @@@ note\n");
    parseSyntaxColor(&Text_);
    Text_.options.script = NULL;
    assert(text_bookmark_note(&Text_, Buffer, NULL) == NULL);
    Text_.options.script = &NoCommentScript;
    assert(text_bookmark_note(&Text_, Buffer, NULL) == NULL);

    puts("bookmark line ok");
}

static void testBookmarkToggle(void)
{
    // Insert above an indented line, at the line's first byte.
    setup("    player.speed = 4\n");
    Text_.cursor.position = Buffer + 6;

    toggleBookmark(&Text_, Buffer + 4);

    assert(strcmp(Buffer, "-- @@@ bookmark\n    player.speed = 4\n") == 0);
    assert(Text_.cursor.position == Buffer + 15);
    assert(Text_.cursor.selection == NULL);

    text_undo(&Text_);
    assert(strcmp(Buffer, "    player.speed = 4\n") == 0);

    // Remove a marker line in the middle, and take it back with one undo.
    setup("aa\n-- @@@ physics\nbb\n");
    parseSyntaxColor(&Text_);

    toggleBookmark(&Text_, Buffer + 3);

    assert(strcmp(Buffer, "aa\nbb\n") == 0);

    text_undo(&Text_);
    assert(strcmp(Buffer, "aa\n-- @@@ physics\nbb\n") == 0);

    // The last line with a final newline.
    setup("aa\n-- @@@ note\n");
    parseSyntaxColor(&Text_);

    toggleBookmark(&Text_, Buffer + 3);
    assert(strcmp(Buffer, "aa\n") == 0);

    // The last line without a final newline: no blank line is left.
    setup("aa\n-- @@@ note");
    parseSyntaxColor(&Text_);

    toggleBookmark(&Text_, Buffer + 3);
    assert(strcmp(Buffer, "aa") == 0);

    // The line end before the last marker line is a CRLF pair.
    setup("aa\r\n-- @@@ note");
    parseSyntaxColor(&Text_);

    toggleBookmark(&Text_, Buffer + 4);
    assert(strcmp(Buffer, "aa") == 0);

    // The only line.
    setup("-- @@@");
    parseSyntaxColor(&Text_);

    toggleBookmark(&Text_, Buffer);
    assert(strcmp(Buffer, "") == 0);

    // An indented line is not a marker: a new marker goes above it.
    setup("  -- @@@ note\n");
    parseSyntaxColor(&Text_);

    toggleBookmark(&Text_, Buffer);
    assert(strcmp(Buffer, "-- @@@ bookmark\n  -- @@@ note\n") == 0);

    // The cap leaves the text alone.
    setup("");
    memset(Buffer, 'a', MAX_CODE);
    Buffer[MAX_CODE] = '\0';

    toggleBookmark(&Text_, Buffer);
    assert(strlen(Buffer) == (size_t)MAX_CODE);

    // A language without a single-line comment does nothing.
    setup("aa\n");
    Text_.options.script = &NoCommentScript;

    toggleBookmark(&Text_, Buffer);
    assert(strcmp(Buffer, "aa\n") == 0);

    puts("bookmark toggle ok");
}

static void testSyntax(void)
{
    setup("-- note\nx = 1\n\"s\"\nif\n");

    parseSyntaxColor(&Text_);

    assert(getState(&Text_, Buffer + 1)->syntax == SyntaxType_COMMENT);
    assert(getState(&Text_, Buffer + 8)->syntax == SyntaxType_FG);
    assert(getState(&Text_, Buffer + 15)->syntax == SyntaxType_STRING);
    assert(getState(&Text_, Buffer + 19)->syntax == SyntaxType_KEYWORD);

    puts("syntax ok");
}

static void testTab(void)
{
    setup("x\n");
    Text_.cursor.position = Buffer;

    assert(text_do_tab(&Text_, false, false));
    assert(strcmp(Buffer, "\tx\n") == 0);

    Options.spacesForTab = true;
    setup("x\n");
    Text_.cursor.position = Buffer;

    assert(text_do_tab(&Text_, false, false));
    assert(strcmp(Buffer, "    x\n") == 0);

    // Untab removes exactly the spaces it added.
    setup("    x\n");
    Text_.cursor.position = Buffer;

    assert(text_do_tab(&Text_, true, false));
    assert(strcmp(Buffer, "x\n") == 0);

    Options.spacesForTab = false;

    puts("tab ok");
}

static void testComment(void)
{
    setup("x = 1\n");
    Text_.cursor.position = Buffer;

    commentLine(&Text_);
    assert(strcmp(Buffer, "--x = 1\n") == 0);

    Text_.cursor.position = Buffer;
    commentLine(&Text_);
    assert(strcmp(Buffer, "x = 1\n") == 0);

    puts("comment ok");
}

static void testDuplicate(void)
{
    setup("ab\ncd\n");
    Text_.cursor.position = Buffer;

    assert(text_duplicate_line(&Text_));
    assert(strcmp(Buffer, "ab\nab\ncd\n") == 0);
    assert(Text_.cursor.position == Buffer + 3);

    // A line that would pass the cap is left alone.
    setup("");
    memset(Buffer, 'a', MAX_CODE - 1);
    Buffer[MAX_CODE - 1] = '\0';
    Text_.cursor.position = Buffer;

    assert(!text_duplicate_line(&Text_));
    assert(strlen(Buffer) == (size_t)MAX_CODE - 1);

    puts("duplicate ok");
}

static void testLines(void)
{
    setup("a\nb\nc");
    assert(getLinesCount(&Text_) == 2);

    setup("a");
    assert(getLinesCount(&Text_) == 0);

    setup("");
    assert(getLinesCount(&Text_) == 0);

    puts("lines ok");
}

static void testTrim(void)
{
    setup("a  \nb\t\n");

    text_trim(&Text_);

    assert(strcmp(Buffer, "a\nb\n") == 0);
    assert(Text_.cursor.position <= Buffer + strlen(Buffer));
    assert(Text_.cursor.selection == NULL);

    puts("trim ok");
}

int main(void)
{
    testCap();
    testState();
    testUndoRedo();
    testNavigation();
    testSyntax();
    testSyntaxOtherLanguage();
    testBookmarkLine();
    testBookmarkToggle();
    testTab();
    testComment();
    testDuplicate();
    testLines();
    testTrim();

    return 0;
}
