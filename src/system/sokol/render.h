#pragma once

#include "studio/system.h"

void render_player_rect(const Studio* studio, float* x, float* y, float* w, float* h);
void render_init(void);
void render_shutdown(void);
void render_frame(const Studio* studio, const u32* framebuffer, bool dirty);
