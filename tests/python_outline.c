/* Offline regression test for the python code editor's outline.
 *
 * The scanner is static inside src/api/python.c, which cannot be linked
 * without pocketpy and the whole API binding, so the test goes through the
 * tic_script the built module exports -- ie exactly what the editor calls.
 *
 * Build python first (-DBUILD_WITH_PYTHON=On), then:
 *   cc -Isrc -Iinclude tests/python_outline.c -ldl -o python-outline-test
 *   ./python-outline-test build/bin/python.so
 */
#include "api.h"
#include "script.h"
#include <assert.h>
#include <dlfcn.h>
#include <stdio.h>
#include <string.h>

static const tic_script* script;

/* The outline carries positions into the source, not copies, and the name is
 * not terminated at item->size -- so compare against the length too. */
static bool has(const tic_outline_item* items, s32 count, const char* name)
{
    for (s32 i = 0; i < count; i++)
        if (items[i].size == (s32)strlen(name) &&
            strncmp(items[i].pos, name, items[i].size) == 0)
            return true;

    return false;
}

static void outline(const char* code, s32 expected, const char* first, ...)
{
    s32 count = -1;
    const tic_outline_item* items = script->getOutline(code, &count);

    if (count != expected)
    {
        fprintf(stderr, "expected %d items, got %d, for:\n%s\n", expected, count, code);
        assert(false);
    }

    if (first && !has(items, count, first))
    {
        fprintf(stderr, "missing '%s' in outline of:\n%s\n", first, code);
        assert(false);
    }
}

int main(int argc, char** argv)
{
    assert(argc == 2 && "usage: python-outline-test <path to python.so>");

    void* module = dlopen(argv[1], RTLD_NOW);
    if (!module)
    {
        fprintf(stderr, "dlopen: %s\n", dlerror());
        return 1;
    }

    script = dlsym(module, "ScriptConfig");
    assert(script && "python module exports no ScriptConfig");
    assert(script->getOutline && "python script has no getOutline");

    /* the shapes a cart actually contains */
    outline("def TIC():\n    pass\n", 1, "TIC");
    outline("def scanline(row):\n    pass\n", 1, "scanline");
    outline("class Player:\n    pass\n", 1, "Player");
    outline("class Enemy(Player):\n    pass\n", 1, "Enemy");

    /* a method is listed like any other def -- the editor sorts, so the
     * grouping this scanner emits in does not matter */
    outline("class P:\n    def update(self):\n        pass\n", 2, "update");
    outline("class P:\n    def update(self):\n        pass\n", 2, "P");

    /* several defs */
    outline("def a():\n    pass\ndef b():\n    pass\ndef c():\n    pass\n", 3, "b");

    /* keywords, not identifier tails */
    outline("undef = 1\n", 0, NULL);
    outline("subclass = 1\n", 0, NULL);
    outline("redefine_all = 2\n", 0, NULL);

    /* extra space between keyword and name */
    outline("def   spaced():\n    pass\n", 1, "spaced");

    /* a name has to actually end somewhere */
    outline("def \n", 0, NULL);
    outline("class\n", 0, NULL);
    outline("def broken\n", 0, NULL);

    /* no definitions at all */
    outline("x = 1\ntrace(x)\n", 0, NULL);
    outline("", 0, NULL);

    /* A def inside a comment or a string is still reported here; the editor
     * drops anything landing on a comment before drawing (code.c), and the
     * other languages' scanners behave the same way. Pinned so a change to
     * that division of labour is deliberate. */
    outline("# def commented():\n", 1, "commented");

    printf("all python outline tests passed\n");

    dlclose(module);
    return 0;
}
