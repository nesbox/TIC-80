#pragma once

#include "studio/system.h"

void render_init(void);
void render_shutdown(void);
void render_frame(const Studio* studio, const u32* framebuffer, bool dirty);
