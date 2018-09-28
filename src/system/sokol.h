#pragma once

#include "sokol_app.h"
#include "sokol_gfx.h"
#include "sokol_time.h"
#include "sokol_audio.h"

#include <stdint.h>

void sokol_gfx_init(int w, int h, int sx, int sy);
void sokol_gfx_draw(const uint32_t* ptr);
