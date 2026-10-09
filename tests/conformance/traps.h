/* Installs the recording wrappers from traps.c over the core's API table. */
#pragma once

#include "core/core.h"

void traps_install(tic_core* core);

// One recorded line; the runner appends it to the current language's record.
void api_record(const char* format, ...);
