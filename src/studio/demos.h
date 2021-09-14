#include "api.h"

typedef struct
{
    const char* name;
    const void* demoRom;
    const s32   demoRomSize;
    const void* markRom;
    const s32   markRomSize;
    const tic_script_demo* demos[];
} tic_script_config_extra;


extern tic_script_config_extra* LanguagesExtra[];

tic_script_config_extra* getConfigExtra(const tic_script_config* config);
