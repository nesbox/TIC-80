#include <string.h>
#include <stddef.h>
#include <stdio.h>
#include "demos.h"
#include "api.h"

#if defined (TIC_BUILD_WITH_LUA)
static const u8 LuaDemoRom[] =
{
    #include "../build/assets/luademo.tic.dat"
};
static const u8 LuaMarkRom[] =
{
    #include "../build/assets/luamark.tic.dat"
};
tic_script_config_extra LuaSyntaxConfigExtra =
{
    .name                  = "lua",
    .demoRom               = LuaDemoRom,
    .demoRomSize           = sizeof LuaDemoRom,
    .markRom               = LuaMarkRom,
    .markRomSize           = sizeof LuaMarkRom,
};
#endif

#if defined (TIC_BUILD_WITH_MOON)
static const u8 MoonDemoRom[] =
{
    #include "../build/assets/moondemo.tic.dat"
};
static const u8 MoonMarkRom[] =
{
    #include "../build/assets/moonmark.tic.dat"
};
tic_script_config_extra MoonSyntaxConfigExtra =
{
    .name               = "moon",
    .demoRom            = MoonDemoRom,
    .demoRomSize        = sizeof MoonDemoRom,
    .markRom            = MoonMarkRom,
    .markRomSize        = sizeof MoonMarkRom,
};
#endif

#if defined (TIC_BUILD_WITH_FENNEL)
static const u8 FennelDemoRom[] =
{
    #include "../build/assets/fenneldemo.tic.dat"
};
/* does not exists
static const u8 FennelMarkRom[] =
{
    #include "../build/assets/fennelmark.tic.dat"
};
*/

tic_script_config_extra FennelSyntaxConfigExtra =
{
    .name               = "fennel",
    .demoRom            = FennelDemoRom,
    .demoRomSize        = sizeof FennelDemoRom,
    .markRom            = NULL,
    .markRomSize        = 0,
};
#endif


#if defined(TIC_BUILD_WITH_JS)

static const u8 JsDemoRom[] =
{
    #include "../build/assets/jsdemo.tic.dat"
};

static const u8 jsmark[] =
{
    #include "../build/assets/jsmark.tic.dat"
};

tic_script_config_extra JsSyntaxConfigExtra =
{
    .name               = "js",
    .demoRom            = JsDemoRom,
    .demoRomSize        = sizeof JsDemoRom,
    .markRom            = jsmark,
    .markRomSize        = sizeof jsmark,
};

#endif

#if defined(TIC_BUILD_WITH_MRUBY)

static const u8 RubyDemoRom[] =
{
    #include "../build/assets/rubydemo.tic.dat"
};

static const u8 rubymark[] =
{
    #include "../build/assets/rubymark.tic.dat"
};

tic_script_config_extra MRubySyntaxConfigExtra =
{
    .name               = "ruby",
    .demoRom            = RubyDemoRom,
    .demoRomSize        = sizeof RubyDemoRom,
    .markRom            = rubymark,
    .markRomSize        = sizeof rubymark,
};

#endif

#if defined(TIC_BUILD_WITH_WREN)

static const u8 WrenDemoRom[] =
{
    #include "../build/assets/wrendemo.tic.dat"
};

static const u8 wrenmark[] =
{
    #include "../build/assets/wrenmark.tic.dat"
};

tic_script_config_extra WrenSyntaxConfigExtra =
{
    .name               = "wren",
    .demoRom            = WrenDemoRom,
    .demoRomSize        = sizeof WrenDemoRom,
    .markRom            = wrenmark,
    .markRomSize        = sizeof wrenmark,
};

#endif

#if defined (TIC_BUILD_WITH_SCHEME)
static const u8 SchemeDemoRom[] =
{
    #include "../build/assets/schemedemo.tic.dat"
};

static const u8 schememark[] =
{
    #include "../build/assets/schememark.tic.dat"
};
tic_script_config_extra SchemeSyntaxConfigExtra =
{
    .name               = "scheme",
    .demoRom            = SchemeDemoRom,
    .demoRomSize        = sizeof SchemeDemoRom,
    .markRom            = schememark,
    .markRomSize        = sizeof schememark,
};
#endif


#if defined (TIC_BUILD_WITH_SQUIRREL)
static const u8 SquirrelDemoRom[] =
{
    #include "../build/assets/squirreldemo.tic.dat"
};

static const u8 squirrelmark[] =
{
    #include "../build/assets/squirrelmark.tic.dat"
};
tic_script_config_extra SquirrelSyntaxConfigExtra =
{
    .name               = "squirrel",
    .demoRom            = SquirrelDemoRom,
    .demoRomSize        = sizeof SquirrelDemoRom,
    .markRom            = squirrelmark,
    .markRomSize        = sizeof squirrelmark,
};
#endif


#if defined(TIC_BUILD_WITH_WASM)

static const u8 WasmDemoRom[] =
{
    #include "../build/assets/wasmdemo.tic.dat"
};

static const u8 wasmmark[] =
{
    #include "../build/assets/wasmmark.tic.dat"
};

tic_script_config_extra WasmSyntaxConfigExtra =
{
    .name               = "wasm",
    .demoRom            = WasmDemoRom,
    .demoRomSize        = sizeof WasmDemoRom,
    .markRom            = wasmmark,
    .markRomSize        = sizeof wasmmark,
};

#endif

#if defined(TIC_BUILD_WITH_JANET)

static const u8 JanetDemoRom[] =
  {
#include "../build/assets/janetdemo.tic.dat"
  };

static const u8 janetmark[] =
  {
#include "../build/assets/janetmark.tic.dat"
  };

tic_script_config_extra JanetSyntaxConfigExtra =
  {
    .name               = "janet",
    .demoRom            = JanetDemoRom,
    .demoRomSize        = sizeof JanetDemoRom,
    .markRom            = janetmark,
    .markRomSize        = sizeof janetmark,
  };

#endif

#if defined(TIC_BUILD_WITH_PYTHON)

static const u8 PythonDemoRom[] =
  {
#include "../build/assets/pythondemo.tic.dat"
  };

static const u8 pythonmark[] =
  {
#include "../build/assets/pythonmark.tic.dat"
  };

tic_script_config_extra PythonSyntaxConfigExtra =
  {
    .name               = "python",
    .demoRom            = PythonDemoRom,
    .demoRomSize        = sizeof PythonDemoRom,
    .markRom            = pythonmark,
    .markRomSize        = sizeof pythonmark,
  };

#endif


tic_script_config_extra* getConfigExtra(const tic_script_config* config)
{

    for (tic_script_config_extra** conf = LanguagesExtra ; *conf != NULL; conf++ ) {
        tic_script_config_extra* ln = *conf;
        if (strcmp(config->name, ln->name) == 0)
        {
            return ln;
        }
    }
    return NULL;
}

tic_script_config_extra* LanguagesExtra[] = {
#if defined(TIC_BUILD_WITH_LUA)
   &LuaSyntaxConfigExtra,
#endif
#if defined(TIC_BUILD_WITH_MOON)
   &MoonSyntaxConfigExtra,
#endif
#if defined(TIC_BUILD_WITH_FENNEL)
   &FennelSyntaxConfigExtra,
#endif
#if defined(TIC_BUILD_WITH_WREN)
   &WrenSyntaxConfigExtra,
#endif
#if defined(TIC_BUILD_WITH_SCHEME)
   &SchemeSyntaxConfigExtra,
#endif

#if defined(TIC_BUILD_WITH_SQUIRREL)
   &SquirrelSyntaxConfigExtra,
#endif
#if defined(TIC_BUILD_WITH_JS)
   &JsSyntaxConfigExtra,
#endif
#if defined(TIC_BUILD_WITH_MRUBY)
   &MRubySyntaxConfigExtra,
#endif
#if defined(TIC_BUILD_WITH_WASM)
   &WasmSyntaxConfigExtra,
#endif
#if defined(TIC_BUILD_WITH_JANET)
   &JanetSyntaxConfigExtra,
#endif
#if defined(TIC_BUILD_WITH_PYTHON)
   &PythonSyntaxConfigExtra,
#endif
   NULL
};
