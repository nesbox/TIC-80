#pragma once

#include "studio/system.h"

// The key each physical position prints, asked of the OS the way SDL does.
// A non-Latin layout keeps its letter positions; ASCII follows the OS.
tic_layout tic_sys_layout_refresh(void);
tic_key tic_sys_layout_key(tic_key position);
