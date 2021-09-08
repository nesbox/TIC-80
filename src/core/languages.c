#include <stddef.h>
#include "api.h"

extern tic_script_config JsSyntaxConfig;
extern tic_script_config LuaSyntaxConfig;
extern tic_script_config MoonSyntaxConfig;
extern tic_script_config FennelSyntaxConfig;
extern tic_script_config SquirrelSyntaxConfig;
extern tic_script_config WrenSyntaxConfig;

tic_script_config* Languages[] = {
&LuaSyntaxConfig, 
&JsSyntaxConfig,
&MoonSyntaxConfig, 
&FennelSyntaxConfig, 
&SquirrelSyntaxConfig,
&WrenSyntaxConfig,
NULL};


