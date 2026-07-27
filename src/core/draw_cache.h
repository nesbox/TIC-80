// MIT License
// Copyright (c) 2026 Vadim Grigoruk @nesbox // grigoruk@gmail.com

#pragma once

#include "core.h"

#define DRAW_CACHE_MAX_SIZE (4 * 1024 * 1024)

void tic_core_draw_cache_init(tic_core* core);
void tic_core_draw_cache_free(tic_core* core);
void tic_core_draw_cache_start(tic_core* core);
void tic_core_draw_cache_end(tic_core* core);
void tic_core_draw_cache_invalidate(tic_core* core);
void tic_core_draw_cache_hook_api(tic_core* core);
bool tic_core_draw_cache_has_invalidated(tic_core* core);

