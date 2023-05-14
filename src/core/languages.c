#include <stddef.h>
#include "api.h"

#if defined(TIC_BUILD_WITH_MRUBY)
extern tic_script_config MRubySyntaxConfig;
#endif

#if defined(TIC_BUILD_WITH_JS)
extern tic_script_config JsSyntaxConfig;
#endif

#if defined (TIC_BUILD_WITH_LUA)
extern tic_script_config LuaSyntaxConfig;
#endif

#if defined(TIC_BUILD_WITH_MOON)
extern tic_script_config MoonSyntaxConfig;
#endif

#if defined(TIC_BUILD_WITH_FENNEL)
extern tic_script_config FennelSyntaxConfig;
#endif

#if defined(TIC_BUILD_WITH_SQUIRREL)
extern tic_script_config SquirrelSyntaxConfig;
#endif

#if defined(TIC_BUILD_WITH_SCHEME)
extern tic_script_config SchemeSyntaxConfig;
#endif

#if defined(TIC_BUILD_WITH_WREN)
extern tic_script_config WrenSyntaxConfig;
#endif

#if defined(TIC_BUILD_WITH_WASM)
extern tic_script_config WasmSyntaxConfig;
#endif

#if defined(TIC_BUILD_WITH_JANET)
extern tic_script_config JanetSyntaxConfig;
#endif

#if defined(TIC_BUILD_WITH_PYTHON)
extern tic_script_config PythonSyntaxConfig;
#endif


tic_script_config* Languages[] = {

	#if defined (TIC_BUILD_WITH_LUA)
	&LuaSyntaxConfig,
	#endif

  #if defined(TIC_BUILD_WITH_MRUBY)
  &MRubySyntaxConfig,
  #endif

	#if defined(TIC_BUILD_WITH_JS)
	&JsSyntaxConfig,
	#endif

	#if defined(TIC_BUILD_WITH_MOON)
	&MoonSyntaxConfig,
	#endif

	#if defined(TIC_BUILD_WITH_FENNEL)
	&FennelSyntaxConfig,
	#endif

    #if defined(TIC_BUILD_WITH_SCHEME)
	&SchemeSyntaxConfig,
	#endif

	#if defined(TIC_BUILD_WITH_SQUIRREL)
	&SquirrelSyntaxConfig,
	#endif

	#if defined(TIC_BUILD_WITH_WREN)
	&WrenSyntaxConfig,
	#endif

  #if defined(TIC_BUILD_WITH_WASM)
	&WasmSyntaxConfig,
	#endif

  #if defined(TIC_BUILD_WITH_JANET)
  &JanetSyntaxConfig,
  #endif

  #if defined(TIC_BUILD_WITH_PYTHON)
  &PythonSyntaxConfig,
  #endif

	NULL};
