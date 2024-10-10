// MIT License

// Copyright (c) 2017 Vadim Grigoruk @nesbox // grigoruk@gmail.com

// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:

// The above copyright notice and this permission notice shall be included in all
// copies or substantial portions of the Software.

// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
// SOFTWARE.

#include "script.h"
#include "tools.h"

#include <stdio.h>

#define MAX_SUPPORTED_LANGS (16)

#if defined(TIC_RUNTIME_STATIC)

#if defined (TIC_BUILD_WITH_LUA)
extern tic_script EXPORT_SCRIPT(Lua);
#endif

#if defined(TIC_BUILD_WITH_RUBY)
extern tic_script EXPORT_SCRIPT(Ruby);
#endif

#if defined(TIC_BUILD_WITH_JS)
extern tic_script EXPORT_SCRIPT(Js);
#endif

#if defined(TIC_BUILD_WITH_MOON)
extern tic_script EXPORT_SCRIPT(Moon);
#endif

#if defined(TIC_BUILD_WITH_FENNEL)
extern tic_script EXPORT_SCRIPT(Fennel);
#endif

#if defined(TIC_BUILD_WITH_SQUIRREL)
extern tic_script EXPORT_SCRIPT(Squirrel);
#endif

#if defined(TIC_BUILD_WITH_SCHEME)
extern tic_script EXPORT_SCRIPT(Scheme);
#endif

#if defined(TIC_BUILD_WITH_WREN)
extern tic_script EXPORT_SCRIPT(Wren);
#endif

#if defined(TIC_BUILD_WITH_WASM)
extern tic_script EXPORT_SCRIPT(Wasm);
#endif

#if defined(TIC_BUILD_WITH_JANET)
extern tic_script EXPORT_SCRIPT(Janet);
#endif

#if defined(TIC_BUILD_WITH_PYTHON)
extern tic_script EXPORT_SCRIPT(Python);
#endif

#endif

static const tic_script *Scripts[MAX_SUPPORTED_LANGS + 1] =
{
#if defined(TIC_RUNTIME_STATIC)
    #if defined (TIC_BUILD_WITH_LUA)
    &EXPORT_SCRIPT(Lua),
    #endif

    #if defined(TIC_BUILD_WITH_RUBY)
    &EXPORT_SCRIPT(Ruby),
    #endif

    #if defined(TIC_BUILD_WITH_JS)
    &EXPORT_SCRIPT(Js),
    #endif

    #if defined(TIC_BUILD_WITH_MOON)
    &EXPORT_SCRIPT(Moon),
    #endif

    #if defined(TIC_BUILD_WITH_FENNEL)
    &EXPORT_SCRIPT(Fennel),
    #endif

    #if defined(TIC_BUILD_WITH_SCHEME)
    &EXPORT_SCRIPT(Scheme),
    #endif

    #if defined(TIC_BUILD_WITH_SQUIRREL)
    &EXPORT_SCRIPT(Squirrel),
    #endif

    #if defined(TIC_BUILD_WITH_WREN)
    &EXPORT_SCRIPT(Wren),
    #endif

    #if defined(TIC_BUILD_WITH_WASM)
    &EXPORT_SCRIPT(Wasm),
    #endif

    #if defined(TIC_BUILD_WITH_JANET)
    &EXPORT_SCRIPT(Janet),
    #endif

    #if defined(TIC_BUILD_WITH_PYTHON)
    &EXPORT_SCRIPT(Python),
    #endif

#endif

    NULL,
};

static s32 compareScripts(const void* a, const void* b)
{
    const tic_script* script1 = *(const tic_script**)a;
    const tic_script* script2 = *(const tic_script**)b;

    if (script1->id < script2->id) return -1;
    if (script1->id > script2->id) return 1;
    return 0;
}

void tic_add_script(const tic_script* script)
{
    s32 index = 0;
    FOREACH_LANG(it)
    {
        if(it->id == script->id || strcmp(it->name, script->name) == 0)
        {
            // script already exists
            return;
        }

        index++;
    }

    if(index < MAX_SUPPORTED_LANGS)
    {
        Scripts[index] = script;
        qsort(Scripts, index + 1, sizeof Scripts[0], (int (*)(const void *, const void *))compareScripts);
    }
}

const tic_script** tic_scripts()
{
    return Scripts;
}

const tic_script* tic_get_script(tic_mem* memory)
{
    FOREACH_LANG(script)
    {
        if(script->id == memory->cart.lang
            || strcmp(tic_tool_metatag(memory->cart.code.data, "script", script->singleComment), script->name) == 0)
            return script;
    }

    static const tic_script empty;

    return *Scripts ? *Scripts : &empty;
}
